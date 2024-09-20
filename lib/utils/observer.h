
#include <vector>
#include <functional>
#include "utils/log.h"


template <typename T> class Observer;


template <typename T>
class Subject {
    std::vector<Observer<T>*> observers;

    void pop_observer(int i) {
        Observer<T>* observer = observers.at(i);

        observers.erase(observers.begin() + i);
        observer->subject = NULL;
        log_debug("Detached observer ", &observer);
    }

public:

    ~Subject() {
        for (int i = observers.size() - 1; i >= 0; i--) {
            pop_observer(i);
        }
    }

    bool attach(Observer<T>& observer) {
        if (observer.subject) return false;

        observer.subject = this;
        observers.push_back(&observer);
        log_debug("Attached observer ", &observer);
        return true;
    }

    void detach(Observer<T>& observer) {
        int size = observers.size();
        for (int i = 0; i < size; i++) {
            if (&observer == observers[i]) {
                pop_observer(i);
                return;
            }
        }
    }

    void notify(const T& value) {
        for (Observer<T>* observer : observers) {
            observer->notify(value);
        }
    }
};


template <typename T>
class Observer {
    Subject<T>* subject = NULL;
    std::function<void(const T&)> func;

    friend class Subject<T>;
public:

    Observer() : func([](const T& _) {}) {}
    Observer(std::function<void(const T&)> func) : func(func) {}

    ~Observer() {
        detach();
    }

    bool detach() {
        if (!subject) return false;

        subject->detach(*this);
        return true;
    }

    void notify(const T& value) {
        func(value);
    }
};