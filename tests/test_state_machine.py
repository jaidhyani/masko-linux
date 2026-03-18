import json
from pathlib import Path
from backend.state_machine import StateMachine, ConditionValue, compare

MASCOT_DIR = Path(__file__).parent.parent / "resources" / "mascots"

def load_masko():
    config = json.loads((MASCOT_DIR / "masko.json").read_text())
    return StateMachine(config)

def test_initial_node():
    sm = load_masko()
    sm.start()
    assert sm.current_node_name == "Idle"

def test_transition_to_working():
    sm = load_masko()
    sm.start()
    transitions = []
    sm.on_transition = lambda edge, video: transitions.append((edge["target"], video))
    sm.set_input("claudeCode::isIdle", ConditionValue(False))
    sm.set_input("claudeCode::isWorking", ConditionValue(True))
    assert len(transitions) == 1
    sm.handle_video_ended()
    assert sm.current_node_name == "Working"

def test_transition_to_needs_attention():
    sm = load_masko()
    sm.start()
    sm.set_input("claudeCode::isIdle", ConditionValue(False))
    sm.set_input("claudeCode::isWorking", ConditionValue(True))
    sm.handle_video_ended()
    transitions = []
    sm.on_transition = lambda edge, video: transitions.append(edge["target"])
    sm.set_input("claudeCode::isAlert", ConditionValue(True))
    sm.handle_video_ended()
    assert sm.current_node_name == "Needs Attention"

def test_no_transition_when_condition_not_met():
    sm = load_masko()
    sm.start()
    sm.set_input("claudeCode::isIdle", ConditionValue(True))
    assert sm.current_node_name == "Idle"

def test_condition_evaluation():
    sm = load_masko()
    sm.start()
    sm.set_input("claudeCode::isIdle", ConditionValue(False))
    sm.set_input("claudeCode::isWorking", ConditionValue(True))
    sm.handle_video_ended()
    assert sm.current_node_name == "Working"
    sm.set_input("claudeCode::isWorking", ConditionValue(False))
    sm.set_input("claudeCode::isIdle", ConditionValue(True))
    sm.handle_video_ended()
    assert sm.current_node_name == "Idle"

def test_loop_count_input():
    sm = load_masko()
    sm.start()
    sm.handle_loop_completed()
    assert sm.inputs["loopCount"].value == 1.0

def test_comparison_operators():
    assert compare(ConditionValue(5.0), ">", ConditionValue(3.0))
    assert compare(ConditionValue(3.0), "==", ConditionValue(3.0))
    assert compare(ConditionValue(3.0), "!=", ConditionValue(5.0))
    assert compare(ConditionValue(3.0), "<=", ConditionValue(3.0))
    assert not compare(ConditionValue(3.0), ">", ConditionValue(5.0))
