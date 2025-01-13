// ThreadSpecificT.swift
// Thread local storage wrapper template
// dbien
// 12JAN2025

import Foundation

final class ThreadSpecific<T> {
    private var key = pthread_key_t()
    
    init() {
        pthread_key_create(&key) { _ in }
    }
    
    deinit {
        pthread_key_delete(key)
    }
    
    var current: T? {
        get {
            pthread_getspecific(key).map { Unmanaged<Box<T>>.fromOpaque($0).takeUnretainedValue().value }
        }
        set {
            if let value = newValue {
                let box = Box(value)
                pthread_setspecific(key, Unmanaged.passRetained(box).toOpaque())
            } else {
                pthread_setspecific(key, nil)
            }
        }
    }
}

private final class Box<T> {
    let value: T
    init(_ value: T) { self.value = value }
} 