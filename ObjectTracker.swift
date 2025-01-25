// ObjectTracker.swift
// Manages tracking of objects and their first/last state transitions
// dbien
// 20MAR2024

import Foundation

// ObjectTrackerStrongRef:
// This is a simple class that tracks a set of objects with strong references and calls closures when the set changes.
class ObjectTrackerStrongRef<T: Hashable> {
  private var m_setObjects = Set<T>()
  private var m_closureOnFirstAdded: ((AnyObject?) -> Void)?
  private var m_closureOnLastRemoved: ((AnyObject?) -> Void)?
  private weak var m_ctxtContext: AnyObject?
  init(
    context: AnyObject? = nil,
    onFirstAdded: ((AnyObject?) -> Void)? = nil,
    onLastRemoved: ((AnyObject?) -> Void)? = nil
  ) {
    self.m_ctxtContext = context
    self.m_closureOnFirstAdded = onFirstAdded
    self.m_closureOnLastRemoved = onLastRemoved
  }
  var isEmpty: Bool { m_setObjects.isEmpty }
  var count: Int { m_setObjects.count }
  var closureOnFirstAdded: ((AnyObject?) -> Void)? {
    get { m_closureOnFirstAdded }
    set { m_closureOnFirstAdded = newValue }
  }
  var closureOnLastRemoved: ((AnyObject?) -> Void)? {
    get { m_closureOnLastRemoved }
    set { m_closureOnLastRemoved = newValue }
  }
  func contains(_ object: T) -> Bool { m_setObjects.contains(object) }
  func add(_ object: T) {
    let wasEmpty = m_setObjects.isEmpty
    m_setObjects.insert(object)
    if wasEmpty, let onFirstAdded = m_closureOnFirstAdded {
      onFirstAdded(m_ctxtContext)
    }
  }
  func remove(_ object: T) {
    m_setObjects.remove(object)
    if m_setObjects.isEmpty, let onLastRemoved = m_closureOnLastRemoved {
      onLastRemoved(m_ctxtContext)
    }
  }
}

// ObjectTrackerWeakRef:
// This is a simple class that tracks a set of objects with weak references and calls closures when the set changes.
class ObjectTrackerWeakRef<T: AnyObject> {
  private var m_setObjects = NSHashTable<T>.weakObjects()
  private var m_closureOnFirstAdded: (() -> Void)?
  private var m_closureOnLastRemoved: (() -> Void)?
  init(
    onFirstAdded: (() -> Void)? = nil,
    onLastRemoved: (() -> Void)? = nil
  ) {
    self.m_closureOnFirstAdded = onFirstAdded
    self.m_closureOnLastRemoved = onLastRemoved
  }
  var isEmpty: Bool { m_setObjects.count == 0 }
  var count: Int { m_setObjects.count }
  var closureOnFirstAdded: (() -> Void)? {
    get { m_closureOnFirstAdded }
    set { m_closureOnFirstAdded = newValue }
  }
  var closureOnLastRemoved: (() -> Void)? {
    get { m_closureOnLastRemoved }
    set { m_closureOnLastRemoved = newValue }
  }
  func contains(_ object: T) -> Bool { m_setObjects.contains(object) }
  func add(_ object: T) {
    let wasEmpty = m_setObjects.count == 0
    m_setObjects.add(object)
    if wasEmpty, let onFirstAdded = m_closureOnFirstAdded {
      onFirstAdded()
    }
  }
  func remove(_ object: T) {
    m_setObjects.remove(object)
    if m_setObjects.count == 0, let onLastRemoved = m_closureOnLastRemoved {
      onLastRemoved()
    }
  }
}

// ObjectTrackerWeakRefWithContext:
// Adds a context that is sent to the closures.
class ObjectTrackerWeakRefWithContext<T: AnyObject> {
  private var m_setObjects = NSHashTable<T>.weakObjects()
  private var m_closureOnFirstAdded: ((AnyObject?) -> Void)?
  private var m_closureOnLastRemoved: ((AnyObject?) -> Void)?
  private weak var m_ctxtContext: AnyObject?
  init(
    context: AnyObject? = nil,
    onFirstAdded: ((AnyObject?) -> Void)? = nil,
    onLastRemoved: ((AnyObject?) -> Void)? = nil
  ) {
    self.m_ctxtContext = context
    self.m_closureOnFirstAdded = onFirstAdded
    self.m_closureOnLastRemoved = onLastRemoved
  }
  var isEmpty: Bool { m_setObjects.count == 0 }
  var count: Int { m_setObjects.count }
  var closureOnFirstAdded: ((AnyObject?) -> Void)? {
    get { m_closureOnFirstAdded }
    set { m_closureOnFirstAdded = newValue }
  }
  var closureOnLastRemoved: ((AnyObject?) -> Void)? {
    get { m_closureOnLastRemoved }
    set { m_closureOnLastRemoved = newValue }
  }
  func contains(_ object: T) -> Bool { m_setObjects.contains(object) }
  func add(_ object: T) {
    let wasEmpty = m_setObjects.count == 0
    m_setObjects.add(object)
    if wasEmpty, let onFirstAdded = m_closureOnFirstAdded {
      onFirstAdded(m_ctxtContext)
    }
  }
  func remove(_ object: T) {
    m_setObjects.remove(object)
    if m_setObjects.count == 0, let onLastRemoved = m_closureOnLastRemoved {
      onLastRemoved(m_ctxtContext)
    }
  }
}
