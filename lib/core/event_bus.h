#pragma once

#include <vector>
#include <functional>
#include "utils/log.h"
#include "utils/observer.h"
#include "core/event.h"


class EventBus {
    std::unordered_map<EventType, Subject<Event>> subjects;
    bool destroyed = false;

public:
    void clear() {
        subjects.clear();
    }

    EventBus() {
        clear();
    }

    ~EventBus() {
        destroyed = true;
        clear();
    }

    template <typename T>
    void attach(Observer<T>& observer) {
        if (destroyed) return;

        EventType type = T::type();

        if (!subjects.contains(type)) {
            subjects.emplace(type, Subject<Event>());
        }

        Subject<T>* subject = reinterpret_cast<Subject<T>*>(&subjects.at(type));
        subject->attach(observer);
    }

    template <typename T>
    void notify(const T& event) {
        if (destroyed) return;

        EventType type = event.type();

        if (!subjects.contains(type)) return;

        Subject<Event>& subject = subjects.at(type);
        subject.notify(event);
    }
};
