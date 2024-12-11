#pragma once

#include <algorithm>
#include <vector>
#include <functional>
#include <iterator>
#include "utils/log.h"


template <typename T> class Observer;


template <typename T>
class Subject {
    std::vector<Observer<T>*> observers;

    void pop_observer(int i) {
        Observer<T>* observer = observers.at(i);

        observers.erase(observers.begin() + i);
        observer->subject = NULL;
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
        // Iterar sobre uma cópia do vetor de observadores
        // Se isso não for feito, pode ser que o callback altere o vetor de observadores, lançando uma exceção
        std::vector<Observer<T>*> vec;
        copy(observers.begin(), observers.end(), back_inserter(vec));

        for (Observer<T>* observer : vec) {
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

    Observer() : func([](const T&) {}) {}
    Observer(std::function<void(const T&)> func) : func(func) {}

    ~Observer() {
        detach();
    }

    void on(std::function<void(const T&)> func) {
        this->func = func;
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