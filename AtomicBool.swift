
// AtomicBool.swift
// dbien
// 16FEB2025
// Atomic boolean implementation

final class AtomicBool {
    private let lock = NSLock()
    private var value: Bool
    
    init(_ value: Bool = false) {
        self.value = value
    }
    
    func get() -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return value
    }
    
    func set(_ newValue: Bool) {
        lock.lock()
        defer { lock.unlock() }
        value = newValue
    }
}
