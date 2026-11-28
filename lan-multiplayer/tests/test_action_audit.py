import unittest

from lan_bridge.action_audit import compare_actions


def capture(role, skew=0):
    return "\n".join(
        f"LAN action selected: role={role} player={player} seq={i} "
        f"tick={i * 100 + skew} name={name} animation={name}.bytes."
        for player in (1, 2)
        for i, name in enumerate(("KatanaStepBack", "JumpUp", "KatanaSlash"), 1)
    )


class ActionAuditTests(unittest.TestCase):
    def test_matching_actions_with_bounded_skew(self):
        self.assertTrue(compare_actions(capture(1), capture(2, 2))["passed"])

    def test_empty_or_missing_attack_is_not_proof(self):
        self.assertFalse(compare_actions("", "")["passed"])
        self.assertFalse(compare_actions(capture(1), capture(2).replace("KatanaSlash", "KatanaIdle"))["passed"])

    def test_excessive_skew_fails(self):
        self.assertFalse(compare_actions(capture(1), capture(2, 20))["passed"])

    def test_wrong_role_and_new_round_are_rejected(self):
        with self.assertRaises(ValueError):
            compare_actions(capture(2), capture(1))
        with self.assertRaises(ValueError):
            compare_actions(capture(1) + "\n" + capture(1), capture(2))
