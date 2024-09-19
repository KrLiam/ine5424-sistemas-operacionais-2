
enum EventType {
    BASE = -1,
};

struct Event {
    static EventType type() { return EventType::BASE; }
};