// IntRegion.swift
// Integer-based region template implemented as a set of rectangles.
// dbien
// 13JAN2025

struct IntRegion<T: FixedWidthInteger & Comparable> {
  // Rectangles sorted by minY, then minX for efficient operations
  private var m_rgrect: [IntRect<T>]
  private let m_fNonIntersecting: Bool
  
  init(fNonIntersecting: Bool = true) {
    m_rgrect = []
    m_fNonIntersecting = fNonIntersecting
  }
  
  init(_ rect: IntRect<T>, fNonIntersecting: Bool = true) {
    m_rgrect = rect.isEmpty ? [] : [rect]
    m_fNonIntersecting = fNonIntersecting
  }
  
  init(_ rects: [IntRect<T>], fNonIntersecting: Bool = true) throws {
    m_fNonIntersecting = fNonIntersecting
    if fNonIntersecting {
      // Check for intersections while building sorted array
      m_rgrect = []
      for rect in rects.filter({ !$0.isEmpty }) {
        if !_canAdd(rect) {
          throw IntRegionError.intersectingRects
        }
        let idx = m_rgrect.lowerBound(value: rect) { 
          $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) 
        }
        m_rgrect.insert(rect, at: idx)
      }
    } else {
      m_rgrect = rects.filter { !$0.isEmpty }
                      .sorted { $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) }
    }
  }
  
  private func _canAdd(_ rect: IntRect<T>) -> Bool {
    guard m_fNonIntersecting else { return true }
    return !m_rgrect.contains { $0.intersects(rect) }
  }
  
  var isEmpty: Bool { m_rgrect.isEmpty }
  var bounds: IntRect<T> {
    guard !isEmpty else { return .zero }
    let minX = m_rgrect.map { $0.minX }.min()!
    let minY = m_rgrect.map { $0.minY }.min()!
    let maxX = m_rgrect.map { $0.maxX }.max()!
    let maxY = m_rgrect.map { $0.maxY }.max()!
    return IntRect(x: minX, y: minY, width: maxX - minX, height: maxY - minY)
  }
  
  var rects: [IntRect<T>] { m_rgrect }
  
  @discardableResult
  mutating func add(_ rect: IntRect<T>) -> Bool {
    guard !rect.isEmpty else { return true }
    guard _canAdd(rect) else { return false }
    
    let idx = m_rgrect.lowerBound(value: rect) { 
      $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) 
    }
    m_rgrect.insert(rect, at: idx)
    return true
  }
  
  @discardableResult
  mutating func add(_ region: IntRegion<T>) -> Bool {
    if m_fNonIntersecting {
      // Check all rects first
      for rect in region.rects {
        if !_canAdd(rect) {
          return false
        }
      }
    }
    
    // Merge sorted arrays maintaining sort order
    var result: [IntRect<T>] = []
    var i = 0
    var j = 0
    while i < m_rgrect.count && j < region.m_rgrect.count {
      let r1 = m_rgrect[i]
      let r2 = region.m_rgrect[j]
      if r1.minY < r2.minY || (r1.minY == r2.minY && r1.minX <= r2.minX) {
        result.append(r1)
        i += 1
      } else {
        result.append(r2)
        j += 1
      }
    }
    result.append(contentsOf: m_rgrect[i...])
    result.append(contentsOf: region.m_rgrect[j...])
    m_rgrect = result
    return true
  }
}

enum IntRegionError: Error {
  case intersectingRects
}