// TypeSafeMap.swift
// dbien
// 04DEC2025
// A type-safe map that uses NSMapTable to store objects.

import Foundation

class TypeSafeMap<t_TyKey: AnyObject, t_TyValue> {
  typealias _TyThis = TypeSafeMap
  private let m_map: NSMapTable<t_TyKey, AnyObject>
  init() {
    m_map = NSMapTable.weakToStrongObjects()
  }
  var count: Int {
    return m_map.count
  }
  func setObject(_ value: t_TyValue, forKey key: t_TyKey) {
    m_map.setObject(value as AnyObject, forKey: key)
  }
  func object(forKey key: t_TyKey) -> t_TyValue? {
    return m_map.object(forKey: key) as? t_TyValue
  }
  func removeObject(forKey key: t_TyKey) {
    m_map.removeObject(forKey: key)
  }
  func removeAllObjects() {
    m_map.removeAllObjects()
  }
  var keyEnumerator: TypeSafeMapEnumerator<t_TyKey> {
    return TypeSafeMapEnumerator(m_map.keyEnumerator())
  }
  func applyClosure(_ closure: (t_TyKey, t_TyValue) -> Void) {
    for key in keyEnumerator {
      if let value = object(forKey: key) {
        closure(key, value)
      }
    }
  }
}

class TypeSafeMapEnumerator<t_TyObj: AnyObject>: Sequence {
  private let m_enumr: NSEnumerator

  init(_ enumr: NSEnumerator) {
    self.m_enumr = enumr
  }

  func nextObject() -> t_TyObj? {
    return m_enumr.nextObject() as? t_TyObj
  }

  func makeIterator() -> Iterator {
    return Iterator(self)
  }

  class Iterator: IteratorProtocol {
    private let m_enumr: TypeSafeMapEnumerator

    init(_ enumr: TypeSafeMapEnumerator) {
      self.m_enumr = enumr
    }

    func next() -> t_TyObj? {
      return m_enumr.nextObject()
    }
  }

  // Keep allObjects for NSEnumerator compatibility
  var allObjects: [t_TyObj] {
    Array(self)  // Uses the Sequence protocol to create array
  }
}
