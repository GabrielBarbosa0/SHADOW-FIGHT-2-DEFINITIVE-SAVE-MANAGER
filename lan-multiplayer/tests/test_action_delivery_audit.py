import unittest

from lan_bridge.action_delivery_audit import compare_delivery


def captures():
    logs = [[], []]
    for owner in (1, 2):
        for seq, name in enumerate(("KatanaStepBack", "JumpUp", "KatanaSlash", "KatanaSlash"), 1):
            logs[owner - 1].append(f"LAN action publish: player={owner} seq={seq} tick={seq * 10} name={name} args=1/1/0/-1.")
            logs[2 - owner].append(f"LAN action applied: player={owner} seq={seq} sourceTick={seq * 10} localTick={seq * 10 + 5} name={name}.")
    return ["\n".join(log) for log in logs]


class DeliveryAuditTests(unittest.TestCase):
    def test_repeated_action_is_a_distinct_event(self):
        report = compare_delivery(*captures())
        self.assertTrue(report["passed"])
        self.assertEqual(report["players"]["2"]["slash_count"], 2)

    def test_empty_is_not_success(self):
        self.assertFalse(compare_delivery("", "")["passed"])

    def test_mismatched_name_fails(self):
        first, second = captures()
        self.assertFalse(compare_delivery(first, second.replace("KatanaSlash", "Wrong"))["passed"])

    def test_duplicate_application_rejected(self):
        first, second = captures()
        with self.assertRaises(ValueError):
            compare_delivery(first, second + "\n" + second.splitlines()[0])

    def test_missing_tail_fails(self):
        first, second = captures()
        self.assertFalse(compare_delivery(first, "\n".join(second.splitlines()[:-1]))["passed"])

    def test_wrong_roles_rejected(self):
        with self.assertRaises(ValueError):
            compare_delivery(*reversed(captures()))

    def test_overflow_rejected(self):
        first, second = captures()
        with self.assertRaises(ValueError):
            compare_delivery(first + "LAN action queue full", second)
