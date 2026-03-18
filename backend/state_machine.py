import json
from dataclasses import dataclass
from typing import Any, Callable, Optional


@dataclass
class ConditionValue:
    value: Any

    def __init__(self, value):
        if isinstance(value, bool):
            self.value = value
        elif isinstance(value, (int, float)):
            self.value = float(value)
        else:
            self.value = value

    @property
    def double_value(self) -> float:
        if isinstance(self.value, bool):
            return 1.0 if self.value else 0.0
        return float(self.value)

    def __eq__(self, other):
        if isinstance(other, ConditionValue):
            return self.double_value == other.double_value
        return NotImplemented


def compare(lhs: ConditionValue, op: str, rhs: ConditionValue) -> bool:
    left, right = lhs.double_value, rhs.double_value
    ops = {"==": left == right, "!=": left != right, ">": left > right,
           "<": left < right, ">=": left >= right, "<=": left <= right}
    return ops.get(op, False)


class StateMachine:
    def __init__(self, config: dict):
        self.config = config
        self.current_node_id = config["initialNode"]
        self.inputs: dict[str, ConditionValue] = {}
        self.phase = "idle"
        self._pending_edge = None
        self._loop_count = 0
        self._node_time_generation = 0

        self.on_transition: Optional[Callable] = None
        self.on_loop_start: Optional[Callable] = None

        self._init_inputs()

    @property
    def current_node_name(self) -> str:
        for node in self.config["nodes"]:
            if node["id"] == self.current_node_id:
                return node["name"]
        return self.current_node_id

    def _init_inputs(self):
        self.inputs["clicked"] = ConditionValue(False)
        self.inputs["mouseOver"] = ConditionValue(False)
        self.inputs["loopCount"] = ConditionValue(0.0)
        self.inputs["nodeTime"] = ConditionValue(0.0)
        self.inputs["claudeCode::isWorking"] = ConditionValue(False)
        self.inputs["claudeCode::isIdle"] = ConditionValue(True)
        self.inputs["claudeCode::isAlert"] = ConditionValue(False)
        self.inputs["claudeCode::isCompacting"] = ConditionValue(False)
        self.inputs["claudeCode::sessionCount"] = ConditionValue(0.0)
        for inp in self.config.get("inputs") or []:
            default = inp.get("default", inp.get("defaultValue"))
            if isinstance(default, bool):
                self.inputs[inp["name"]] = ConditionValue(default)
            else:
                self.inputs[inp["name"]] = ConditionValue(float(default) if default is not None else 0.0)

    def start(self):
        self._arrive_at_node(self.current_node_id)

    def set_input(self, name: str, value: ConditionValue):
        old = self.inputs.get(name)
        if old is not None and old == value:
            return
        self.inputs[name] = value
        self._evaluate_and_fire(name)

    def handle_loop_completed(self):
        if self.phase != "looping":
            return
        self._loop_count += 1
        self.set_input("loopCount", ConditionValue(float(self._loop_count)))

    def handle_video_ended(self):
        if self.phase != "transitioning" or not self._pending_edge:
            return
        target = self._pending_edge["target"]
        self._pending_edge = None
        self._arrive_at_node(target)

    def _evaluate_and_fire(self, changed_input: str):
        if self.phase not in ("looping", "idle"):
            return
        for edge in self.config["edges"]:
            if edge["source"] != self.current_node_id or edge.get("isLoop", False):
                continue
            conditions = edge.get("conditions") or []
            if not conditions:
                continue
            if self._evaluate_conditions(conditions):
                self._reset_trigger(changed_input)
                self._play_transition(edge)
                return

    def _evaluate_conditions(self, conditions: list[dict]) -> bool:
        for cond in conditions:
            inp_val = self.inputs.get(cond["input"])
            if inp_val is None:
                return False
            op = cond.get("op", "==")
            raw_val = cond.get("value", True)
            cond_val = ConditionValue(raw_val)
            if not compare(inp_val, op, cond_val):
                return False
        return True

    def _reset_trigger(self, name: str):
        if name == "clicked":
            self.inputs["clicked"] = ConditionValue(False)
        custom_inputs = self.config.get("inputs") or []
        for inp in custom_inputs:
            if inp["name"] == name and inp.get("type") == "trigger":
                self.inputs[name] = ConditionValue(False)

    def _arrive_at_node(self, node_id: str):
        self._node_time_generation += 1
        self._loop_count = 0
        self.current_node_id = node_id
        self.inputs["loopCount"] = ConditionValue(0.0)
        self.inputs["nodeTime"] = ConditionValue(0.0)
        self.inputs["clicked"] = ConditionValue(False)

        loop_edge = None
        for edge in self.config["edges"]:
            if edge["source"] == node_id and edge["target"] == node_id and edge.get("isLoop"):
                loop_edge = edge
                break

        if loop_edge:
            video_url = loop_edge.get("videos", {}).get("webm") or loop_edge.get("videos", {}).get("hevc")
            self.phase = "looping"
            if self.on_loop_start:
                self.on_loop_start(video_url)
        else:
            self.phase = "idle"

        self._evaluate_and_fire("nodeArrival")

    def _play_transition(self, edge: dict):
        video_url = edge.get("videos", {}).get("webm") or edge.get("videos", {}).get("hevc")
        if not video_url:
            self._arrive_at_node(edge["target"])
            return
        self._pending_edge = edge
        self.phase = "transitioning"
        if self.on_transition:
            self.on_transition(edge, video_url)
