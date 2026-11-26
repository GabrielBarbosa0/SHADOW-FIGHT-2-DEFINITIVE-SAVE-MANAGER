from __future__ import annotations

import socket
import unittest

from lan_bridge.__main__ import mirror_fight_state, profile_with_equipment, pulse_buttons
from lan_bridge.equipment import EQUIPMENT_PRESET_NAMES, select_match_presets
from lan_bridge.peer import LanPeer, PeerMode
from lan_bridge.protocol import (
    Button,
    FighterProfile,
    FightAuthority,
    FightConfig,
    FightStart,
    FightState,
    InputState,
    MatchConfig,
    PacketType,
    PlayerRole,
    ProtocolError,
    compare_fight_states,
    decode_packet,
    encode_packet,
    decode_player_role,
    encode_player_role,
    session_digest,
)
from lan_bridge.relay import LanRoomRelay


class ProtocolTests(unittest.TestCase):
    def test_packet_round_trip(self) -> None:
        encoded = encode_packet(PacketType.PING, 17, "sala-secreta", b"teste", timestamp_ms=1234)
        packet = decode_packet(encoded, "sala-secreta")
        self.assertEqual(packet.kind, PacketType.PING)
        self.assertEqual(packet.sequence, 17)
        self.assertEqual(packet.timestamp_ms, 1234)
        self.assertEqual(packet.payload, b"teste")

    def test_wrong_room_token_is_rejected(self) -> None:
        encoded = encode_packet(PacketType.HELLO, 0, "token-correto")
        with self.assertRaisesRegex(ProtocolError, "Token"):
            decode_packet(encoded, "token-errado")

    def test_session_digest_matches_android_implementation(self) -> None:
        self.assertEqual(session_digest("sala-secreta").hex(), "9209259ab1baa75010512d576b3e0427")

    def test_input_state_round_trip(self) -> None:
        original = InputState(0.75, -1.0, Button.PUNCH | Button.MAGIC)
        decoded = InputState.decode(original.encode())
        self.assertAlmostEqual(decoded.horizontal, original.horizontal)
        self.assertAlmostEqual(decoded.vertical, original.vertical)
        self.assertEqual(decoded.buttons, original.buttons)

    def test_invalid_axis_is_rejected(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "horizontal"):
            InputState(1.5, 0.0).encode()

    def test_fighter_profile_round_trip(self) -> None:
        original = FighterProfile(
            level=52,
            name="NAME_SHADOW",
            avatar="avatar_human",
            voice="Male",
            skeleton="Skeleton",
            armor="ARMOR_SHOGUN",
            helm="HELM_CRESCENT",
            weapon="WEAPON_SHOGUN_KATANA",
            ranged="RANGED_CHAKRAM_OF_MASTER",
            magic="MAGIC_ASTEROID",
        )
        self.assertEqual(FighterProfile.decode(original.encode()), original)

    def test_fight_start_round_trip(self) -> None:
        original = FightStart(round_id=0x12345678, delay_ms=2000)
        self.assertEqual(FightStart.decode(original.encode()), original)

    def test_player_role_round_trip(self) -> None:
        self.assertEqual(
            decode_player_role(encode_player_role(PlayerRole.PLAYER_TWO)),
            PlayerRole.PLAYER_TWO,
        )

    def test_fight_config_round_trip(self) -> None:
        original = FightConfig(
            mode=7,
            fingerprint=0xFEDCBA9876543210,
            identity="030=Tournament|duel|1;038=ZONE_1;",
        )
        self.assertEqual(FightConfig.decode(original.encode()), original)

    def test_fight_config_rejects_empty_identity(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "Identidade"):
            FightConfig(mode=7, fingerprint=1, identity="").encode()

    def test_match_config_round_trip_and_fingerprint(self) -> None:
        original = MatchConfig(37, 0x123456789ABCDEF0, "shogun", "titan")
        decoded = MatchConfig.decode(original.encode())
        self.assertEqual(decoded, original)
        self.assertEqual(decoded.fingerprint, original.fingerprint)
        self.assertNotEqual(decoded.fingerprint, 0)

    def test_match_preset_selection_is_reproducible_and_allows_duplicates(self) -> None:
        self.assertEqual(select_match_presets(12345), select_match_presets(12345))
        duplicate_seed = next(
            seed for seed in range(1, 10_000)
            if len(set(select_match_presets(seed))) == 1
        )
        selected = select_match_presets(duplicate_seed)
        self.assertEqual(selected[0], selected[1])
        self.assertIn(selected[0], EQUIPMENT_PRESET_NAMES)

    def test_fight_start_rejects_unsafe_delay(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "Atraso"):
            FightStart(round_id=1, delay_ms=100).encode()

    def test_fight_state_round_trip(self) -> None:
        original = FightState(
            round_id=37,
            tick=1234,
            time_left_seconds=149.0,
            time_left_frames=8940.0,
            left_life=1.0,
            left_hit_life=0.75,
            right_life=0.5,
            right_hit_life=0.25,
            time_passed_frames=60.0,
            time_total_frames=9000.0,
        )
        self.assertEqual(FightState.decode(original.encode()), original)

    def test_fight_authority_round_trip(self) -> None:
        original = FightAuthority(
            round_id=37,
            tick=1234,
            player_damage=0.25,
            opponent_damage=0.75,
            player_rounds_lost=0,
            opponent_rounds_lost=1,
        )
        self.assertEqual(FightAuthority.decode(original.encode()), original)

    def test_fight_authority_rejects_invalid_damage(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "Dano"):
            FightAuthority(1, 1, 1.25, 0.0, 0, 0).encode()

    def test_fight_state_rejects_non_finite_value(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "float"):
            FightState(1, 1, float("nan"), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0).encode()

    def test_fight_state_rejects_zero_tick(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "Tick"):
            FightState(1, 0, 10.0, 20.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0).encode()

    def test_fight_state_comparison_uses_canonical_peer_sides(self) -> None:
        local = FightState(7, 120, 148.0, 8880.0, 0.75, 0.80, 0.40, 0.45, 120.0, 9000.0)
        remote = FightState(7, 118, 148.0, 8882.0, 0.75, 0.80, 0.40, 0.45, 118.0, 9000.0)
        comparison = compare_fight_states(local, remote, remote_age_ms=35)
        self.assertEqual(comparison.timer_frames_delta, -2.0)
        self.assertEqual(comparison.local_fighter_life_delta, 0.0)
        self.assertEqual(comparison.remote_fighter_life_delta, 0.0)
        self.assertTrue(comparison.within_tolerance)

    def test_mirror_fight_state_preserves_canonical_sides(self) -> None:
        local = FightState(9, 44, 149.0, 8940.0, 0.80, 0.85, 0.30, 0.35, 60.0, 9000.0)
        remote = mirror_fight_state(local)
        comparison = compare_fight_states(local, remote)
        self.assertEqual(remote.left_life, local.left_life)
        self.assertEqual(remote.right_life, local.right_life)
        self.assertEqual(comparison.max_life_delta, 0.0)
        self.assertTrue(comparison.within_tolerance)

    def test_fight_state_comparison_detects_divergence_and_stale_state(self) -> None:
        local = FightState(8, 200, 145.0, 8700.0, 1.0, 1.0, 0.25, 0.25, 300.0, 9000.0)
        remote = FightState(8, 160, 147.0, 8820.0, 0.90, 0.90, 0.50, 0.50, 180.0, 9000.0)
        comparison = compare_fight_states(local, remote, remote_age_ms=800)
        self.assertEqual(comparison.max_life_delta, 0.25)
        self.assertFalse(comparison.within_tolerance)

    def test_fight_state_comparison_rejects_mismatched_rounds(self) -> None:
        first = FightState(1, 1, 150.0, 9000.0, 1.0, 1.0, 1.0, 1.0, 0.0, 9000.0)
        second = FightState(2, 1, 150.0, 9000.0, 1.0, 1.0, 1.0, 1.0, 0.0, 9000.0)
        with self.assertRaisesRegex(ProtocolError, "rodadas diferentes"):
            compare_fight_states(first, second)

    def test_truncated_fighter_profile_is_rejected(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "perfil"):
            FighterProfile.decode(b"\x01\x34\x05abc")

    def test_mirror_profile_can_use_a_different_boss_set(self) -> None:
        original = FighterProfile(
            level=52,
            name="NAME_SHADOW",
            avatar="avatar_human",
            voice="Male",
            skeleton="Skeleton",
            armor="BODY_TITAN",
            helm="HEAD_TITAN",
            weapon="WEAPON_TITAN_GIANT_SWORD",
            ranged="RANGED_TITANS_HARPOON",
            magic="MAGIC_MIND_THROW",
        )
        mirrored = profile_with_equipment(original, "lynx")
        self.assertEqual(mirrored.level, original.level)
        self.assertEqual(mirrored.name, original.name)
        self.assertEqual(mirrored.avatar, "boss_lynx")
        self.assertEqual(mirrored.weapon, "WEAPON_LYNX_CLAWS")
        self.assertEqual(mirrored.armor, "BODY_LYNX")

    def test_button_pulse_alternates_pressed_and_released(self) -> None:
        buttons = Button.PUNCH | Button.KICK
        self.assertEqual(pulse_buttons(buttons, 0.0, 2.0), buttons)
        self.assertEqual(pulse_buttons(buttons, 0.25, 2.0), Button(0))
        self.assertEqual(pulse_buttons(buttons, 0.5, 2.0), buttons)
        self.assertEqual(pulse_buttons(buttons, 99.0, 0.0), buttons)


class PeerTests(unittest.TestCase):
    def setUp(self) -> None:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(("127.0.0.1", 0))
        self.port = probe.getsockname()[1]
        probe.close()
        self.host = LanPeer(PeerMode.HOST, "teste-local", bind_host="127.0.0.1", port=self.port, device_name="host")
        self.client = LanPeer(
            PeerMode.CLIENT,
            "teste-local",
            bind_host="127.0.0.1",
            port=self.port,
            server_host="127.0.0.1",
            device_name="client",
        )
        self.addCleanup(self.host.close)
        self.addCleanup(self.client.close)

    def test_handshake_and_input_delivery(self) -> None:
        self.client.start()
        host_events = self.host.poll(1.0)
        client_events = self.client.poll(1.0)

        self.assertEqual(host_events[0].name, "connected")
        self.assertEqual(host_events[0].detail, "client")
        self.assertEqual(client_events[0].name, "connected")
        self.assertEqual(client_events[0].detail, "host")
        self.assertTrue(self.host.connected)
        self.assertTrue(self.client.connected)

        sent = InputState(-1.0, 0.5, Button.KICK | Button.RANGED)
        self.client.send_input(sent)
        input_event = self.host.poll(1.0)[0]
        self.assertEqual(input_event.name, "input")
        self.assertEqual(input_event.input_state, sent)

        host_sent = InputState(0.25, -0.75, Button.PUNCH | Button.BOSS_ABILITY)
        self.host.send_input(host_sent)
        client_input_event = self.client.poll(1.0)[0]
        self.assertEqual(client_input_event.name, "input")
        self.assertEqual(client_input_event.input_state, host_sent)

        profile = FighterProfile(52, "NAME_SHADOW", "avatar_human", "Male", "Skeleton",
            "BODY_TITAN", "HEAD_TITAN", "WEAPON_TITAN_GIANT_SWORD",
            "RANGED_TITANS_HARPOON", "MAGIC_MIND_THROW")
        self.client.send_profile(profile)
        profile_event = self.host.poll(1.0)[0]
        self.assertEqual(profile_event.name, "profile")
        self.assertEqual(profile_event.fighter_profile, profile)

        self.host.send_ready()
        ready_event = self.client.poll(1.0)[0]
        self.assertEqual(ready_event.name, "ready")

        self.client._send(PacketType.READY, b"invalido", self.client.peer_address)
        rejected_ready = self.host.poll(1.0)[0]
        self.assertEqual(rejected_ready.name, "rejected")

        self.client.send_fight_ready()
        self.assertEqual(self.host.poll(1.0)[0].name, "fight-ready")

        start = FightStart(round_id=37, delay_ms=2000)
        self.host.send_start(start)
        start_event = self.client.poll(1.0)[0]
        self.assertEqual(start_event.name, "start")
        self.assertEqual(start_event.fight_start, start)

        self.client.send_start_ack(start.round_id)
        ack_event = self.host.poll(1.0)[0]
        self.assertEqual(ack_event.name, "start-ack")
        self.assertEqual(ack_event.round_id, start.round_id)

        state = FightState(37, 42, -1.0, 2.0, 1.0, 0.9, 0.8, 0.7, 0.6, 0.5)
        self.client.send_state(state)
        state_event = self.host.poll(1.0)[0]
        self.assertEqual(state_event.name, "state")
        self.assertIsNotNone(state_event.fight_state)
        self.assertEqual(state_event.fight_state.encode(), state.encode())

    def test_host_accepts_new_source_port_after_timeout(self) -> None:
        self.client.start()
        self.assertEqual(self.host.poll(1.0)[0].name, "connected")
        self.assertEqual(self.client.poll(1.0)[0].name, "connected")
        old_address = self.host.peer_address

        timeout_events = self.host.tick(self.host._last_receive + self.host.PEER_TIMEOUT + 0.01)
        self.assertEqual(timeout_events[0].name, "timeout")
        self.assertEqual(timeout_events[0].address, old_address)
        self.assertFalse(self.host.connected)
        self.assertIsNone(self.host.peer_address)

        replacement = LanPeer(
            PeerMode.CLIENT,
            "teste-local",
            bind_host="127.0.0.1",
            port=self.port,
            server_host="127.0.0.1",
            device_name="replacement",
        )
        self.addCleanup(replacement.close)
        replacement.start()
        host_events = self.host.poll(1.0)
        replacement_events = replacement.poll(1.0)

        self.assertEqual(host_events[0].name, "connected")
        self.assertEqual(host_events[0].detail, "replacement")
        self.assertEqual(replacement_events[0].name, "connected")
        self.assertNotEqual(self.host.peer_address, old_address)


class RelayTests(unittest.TestCase):
    def setUp(self) -> None:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(("127.0.0.1", 0))
        self.port = probe.getsockname()[1]
        probe.close()
        second_probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        second_probe.bind(("127.0.0.1", 0))
        self.second_port = second_probe.getsockname()[1]
        second_probe.close()
        self.relay = LanRoomRelay(
            "sala-relay", bind_host="127.0.0.1", port=self.port,
            second_port=self.second_port, name="relay",
        )
        self.player_one = LanPeer(
            PeerMode.CLIENT, "sala-relay", bind_host="127.0.0.1", port=self.port,
            server_host="127.0.0.1", device_name="player-one",
        )
        self.player_two = LanPeer(
            PeerMode.CLIENT, "sala-relay", bind_host="127.0.0.1", port=self.second_port,
            server_host="127.0.0.1", device_name="player-two",
        )
        self.addCleanup(self.player_two.close)
        self.addCleanup(self.player_one.close)
        self.addCleanup(self.relay.close)

    def connect(self, player: LanPeer) -> None:
        player.start()
        self.assertEqual(self.relay.poll(1.0)[0].name, "joined")
        self.assertEqual(player.poll(1.0)[0].name, "connected")
        role_event = player.poll(1.0)[0]
        self.assertEqual(role_event.name, "player-role")
        expected_role = PlayerRole.PLAYER_ONE if player is self.player_one else PlayerRole.PLAYER_TWO
        self.assertEqual(role_event.player_role, expected_role)

    def test_forwards_profiles_ready_and_input_between_two_clients(self) -> None:
        self.connect(self.player_one)
        self.connect(self.player_two)
        self.assertEqual(len(self.relay.participants), 2)

        profile = FighterProfile(
            52, "NAME_SHADOW", "avatar_human", "Male", "Skeleton",
            "BODY_TITAN", "HEAD_TITAN", "WEAPON_TITAN_GIANT_SWORD",
            "RANGED_TITANS_HARPOON", "MAGIC_MIND_THROW",
        )
        self.player_one.send_profile(profile)
        self.assertEqual(self.relay.poll(1.0)[0].detail, "PROFILE")
        self.assertEqual(self.player_two.poll(1.0)[0].fighter_profile, profile)

        self.player_two.send_ready()
        self.assertEqual(self.relay.poll(1.0)[0].detail, "READY")
        self.assertEqual(self.player_one.poll(1.0)[0].name, "ready")

        state = InputState(-0.5, 1.0, Button.KICK | Button.MAGIC)
        self.player_one.send_input(state)
        self.assertEqual(self.relay.poll(1.0)[0].detail, "INPUT")
        self.assertEqual(self.player_two.poll(1.0)[0].input_state, state)

        self.player_two.send_fight_ready()
        self.assertEqual(self.relay.poll(1.0)[0].detail, "FIGHT_READY")
        self.assertEqual(self.player_one.poll(1.0)[0].name, "fight-ready")

        start = FightStart(round_id=91, delay_ms=1500)
        self.player_one.send_start(start)
        self.assertEqual(self.relay.poll(1.0)[0].detail, "START")
        self.assertEqual(self.player_two.poll(1.0)[0].fight_start, start)

        self.player_two.send_start_ack(start.round_id)
        self.assertEqual(self.relay.poll(1.0)[0].detail, "START_ACK")
        self.assertEqual(self.player_one.poll(1.0)[0].round_id, start.round_id)

        state = FightState(91, 7, 3.0, 0.0, 1.0, 0.8, 0.6, 0.4, 0.2, 0.0)
        self.player_one.send_state(state)
        self.assertEqual(self.relay.poll(1.0)[0].detail, "STATE")
        received_state = self.player_two.poll(1.0)[0].fight_state
        self.assertIsNotNone(received_state)
        self.assertEqual(received_state.encode(), state.encode())

    def test_dual_port_roles_do_not_depend_on_join_order(self) -> None:
        self.connect(self.player_two)
        self.connect(self.player_one)

    def test_relay_creates_one_authoritative_match_config_for_both_players(self) -> None:
        self.relay.close()
        self.relay = LanRoomRelay(
            "sala-relay",
            bind_host="127.0.0.1",
            port=self.port,
            second_port=self.second_port,
            name="relay",
            fight_start_delay_ms=1500,
        )
        self.addCleanup(self.relay.close)
        self.connect(self.player_one)
        self.connect(self.player_two)

        player_one_profile = FighterProfile(
            52, "P1", "avatar_human", "Male", "Skeleton",
            "Body", "Head", "Fists", "NoRanged", "NoMagic",
        )
        player_two_profile = FighterProfile(
            31, "P2", "avatar_human", "Male", "Skeleton",
            "Body", "Head", "Fists", "NoRanged", "NoMagic",
        )
        self.player_one.send_profile(player_one_profile)
        self.assertEqual(self.relay.poll(1.0)[0].name, "forwarded")
        self.assertEqual(self.player_two.poll(1.0)[0].fighter_profile, player_one_profile)

        self.player_two.send_profile(player_two_profile)
        created = self.relay.poll(1.0)[0]
        self.assertEqual(created.name, "match-config-created")
        self.assertEqual(self.player_one.poll(1.0)[0].fighter_profile, player_two_profile)
        player_one_config = self.player_one.poll(1.0)[0].match_config
        player_two_config = self.player_two.poll(1.0)[0].match_config
        self.assertIsNotNone(player_one_config)
        self.assertEqual(player_one_config, player_two_config)
        self.assertEqual(player_one_config.match_level, 31)
        self.assertIn(player_one_config.player_one_preset, EQUIPMENT_PRESET_NAMES)
        self.assertIn(player_one_config.player_two_preset, EQUIPMENT_PRESET_NAMES)

        fingerprint = player_one_config.fingerprint
        self.player_one.send_match_config_ack(fingerprint)
        self.assertEqual(self.relay.poll(1.0)[0].name, "match-config-ack")
        self.player_two.send_match_config_ack(fingerprint)
        self.assertEqual(self.relay.poll(1.0)[0].name, "match-config-confirmed")
        self.assertEqual(self.player_one.poll(1.0)[0].match_config_fingerprint, fingerprint)
        self.assertEqual(self.player_two.poll(1.0)[0].match_config_fingerprint, fingerprint)

    def test_coordinates_start_for_two_client_only_android_peers(self) -> None:
        self.relay.close()
        self.relay = LanRoomRelay(
            "sala-relay",
            bind_host="127.0.0.1",
            port=self.port,
            second_port=self.second_port,
            name="relay",
            fight_start_delay_ms=1500,
        )
        self.addCleanup(self.relay.close)
        self.connect(self.player_one)
        self.connect(self.player_two)

        config = FightConfig(7, 0x123456789ABCDEF0, "030=Tournament|duel|1;")
        self.player_one.send_fight_config(config)
        self.assertEqual(self.relay.poll(1.0)[0].name, "fight-config")
        self.assertEqual(self.player_two.poll(1.0)[0].fight_config, config)
        self.player_two.send_fight_config(config)
        self.assertEqual(self.relay.poll(1.0)[0].name, "fight-config-matched")
        self.assertEqual(self.player_one.poll(1.0)[0].fight_config, config)

        self.player_one.send_fight_ready()
        self.assertEqual(self.relay.poll(1.0)[0].name, "forwarded")
        self.assertEqual(self.player_two.poll(1.0)[0].name, "fight-ready")

        self.player_two.send_fight_ready()
        started = self.relay.poll(1.0)[0]
        self.assertEqual(started.name, "started")

        player_one_events = self.player_one.poll(1.0) + self.player_one.poll(1.0)
        player_two_events = self.player_two.poll(1.0)
        player_one_start = next(event for event in player_one_events if event.name == "start")
        player_two_start = next(event for event in player_two_events if event.name == "start")
        self.assertIsNotNone(player_one_start.fight_start)
        self.assertEqual(player_one_start.fight_start, player_two_start.fight_start)
        self.assertEqual(player_one_start.fight_start.delay_ms, 1500)

        round_id = player_one_start.fight_start.round_id
        self.player_one.send_start_ack(round_id)
        first_ack = self.relay.poll(1.0)[0]
        self.assertEqual(first_ack.name, "start-ack")
        self.player_two.send_start_ack(round_id)
        second_ack = self.relay.poll(1.0)[0]
        self.assertEqual(second_ack.name, "start-confirmed")
        self.assertIn("acknowledgements=2/2", second_ack.detail)

    def test_relay_blocks_start_when_fight_configs_differ(self) -> None:
        self.relay.close()
        self.relay = LanRoomRelay(
            "sala-relay",
            bind_host="127.0.0.1",
            port=self.port,
            second_port=self.second_port,
            name="relay",
            fight_start_delay_ms=1500,
        )
        self.addCleanup(self.relay.close)
        self.connect(self.player_one)
        self.connect(self.player_two)

        mountain = FightConfig(7, 1, "030=Mountain;")
        tournament = FightConfig(7, 2, "030=Tournament;")
        self.player_one.send_fight_config(mountain)
        self.assertEqual(self.relay.poll(1.0)[0].name, "fight-config")
        self.assertEqual(self.player_two.poll(1.0)[0].fight_config, mountain)
        self.player_two.send_fight_config(tournament)
        mismatch = self.relay.poll(1.0)[0]
        self.assertEqual(mismatch.name, "fight-config-mismatch")
        self.assertIn("P1=", mismatch.detail)
        self.assertIn("P2=", mismatch.detail)
        self.assertEqual(self.player_one.poll(1.0)[0].fight_config, tournament)

        self.player_one.send_fight_ready()
        blocked = self.relay.poll(1.0)[0]
        self.assertEqual(blocked.name, "fight-config-mismatch")
        self.assertIsNone(self.relay._active_start)


if __name__ == "__main__":
    unittest.main()
