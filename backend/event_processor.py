from backend.models import ClaudeEvent, HookEventType
from backend.stores.event import EventStore
from backend.stores.session import SessionStore
from backend.stores.notification import NotificationStore, AppNotification
from backend.state_machine import StateMachine, ConditionValue


class EventProcessor:
    def __init__(self, event_store: EventStore, session_store: SessionStore,
                 notification_store: NotificationStore, state_machine: StateMachine):
        self.event_store = event_store
        self.session_store = session_store
        self.notification_store = notification_store
        self.state_machine = state_machine

    def process(self, event: ClaudeEvent):
        self.event_store.add(event)
        self.session_store.handle_event(event)

        et = event.event_type
        if et == HookEventType.SESSION_START:
            self.state_machine.set_input("claudeCode::isWorking", ConditionValue(True))
            self.state_machine.set_input("claudeCode::isIdle", ConditionValue(False))
            count = len(self.session_store.active_sessions())
            self.state_machine.set_input("claudeCode::sessionCount", ConditionValue(float(count)))

        elif et == HookEventType.SESSION_END:
            active = self.session_store.active_sessions()
            if not active:
                self.state_machine.set_input("claudeCode::isWorking", ConditionValue(False))
                self.state_machine.set_input("claudeCode::isIdle", ConditionValue(True))
            self.state_machine.set_input("claudeCode::sessionCount", ConditionValue(float(len(active))))

        elif et == HookEventType.PERMISSION_REQUEST:
            self.state_machine.set_input("claudeCode::isAlert", ConditionValue(True))

        elif et == HookEventType.PRE_COMPACT:
            self.state_machine.set_input("claudeCode::isCompacting", ConditionValue(True))

        elif et == HookEventType.NOTIFICATION:
            self.notification_store.add(AppNotification(
                title=event.title or "Notification",
                message=event.message or "",
                category=event.notification_type or "info",
            ))
