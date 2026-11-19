from __future__ import annotations

import socket
import unittest

from lan_bridge.peer import LanPeer, PeerMode
from lan_bridge.protocol import (
    Button,
    FighterProfile,
    InputState,
    PacketType,
    ProtocolError,
    decode_packet,
    encode_packet,
    session_digest,
)


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

    def test_truncated_fighter_profile_is_rejected(self) -> None:
        with self.assertRaisesRegex(ProtocolError, "perfil"):
            FighterProfile.decode(b"\x01\x34\x05abc")


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


if __name__ == "__main__":
    unittest.main()
