from backend.stores.notification import NotificationStore, AppNotification

def test_add_notification():
    store = NotificationStore()
    store.add(AppNotification(title="Test", message="Hello", category="info"))
    assert len(store.all()) == 1
    assert store.unread_count() == 1

def test_mark_read():
    store = NotificationStore()
    store.add(AppNotification(title="Test", message="Hello", category="info"))
    nid = store.all()[0].id
    store.mark_read(nid)
    assert store.unread_count() == 0

def test_dismiss():
    store = NotificationStore()
    store.add(AppNotification(title="Test", message="Hello", category="info"))
    nid = store.all()[0].id
    store.dismiss(nid)
    assert len(store.all()) == 0

def test_clear_all():
    store = NotificationStore()
    for i in range(5):
        store.add(AppNotification(title=f"N{i}", message="", category="info"))
    store.clear_all()
    assert len(store.all()) == 0
