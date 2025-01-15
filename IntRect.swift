// IntRect.swift
// Integer-based rectangle template.
// dbien
// 13JAN2025

struct IntRect<T: FixedWidthInteger & Comparable> {
  var origin: IntPoint<T>
  var size: IntSize<T>
  
  init(origin: IntPoint<T> = .zero, size: IntSize<T> = .zero) {
    self.origin = origin
    self.size = size
  }
  
  init(x: T, y: T, width: T, height: T) {
    self.origin = IntPoint(x: x, y: y)
    self.size = IntSize(width: width, height: height)
  }
  
  static var zero: IntRect { IntRect(origin: .zero, size: .zero) }
  
  var minX: T { origin.x }
  var minY: T { origin.y }
  var maxX: T { origin.x + size.width }
  var maxY: T { origin.y + size.height }
  var width: T { size.width }
  var height: T { size.height }
  
  var isEmpty: Bool { size.width <= 0 || size.height <= 0 }
  
  func intersects(_ other: IntRect) -> Bool {
    guard !isEmpty && !other.isEmpty else { return false }
    return maxX > other.minX && minX < other.maxX &&
           maxY > other.minY && minY < other.maxY
  }
  
  func intersection(_ other: IntRect) -> IntRect {
    let newMinX = Swift.max(minX, other.minX)
    let newMinY = Swift.max(minY, other.minY)
    let newMaxX = Swift.min(maxX, other.maxX)
    let newMaxY = Swift.min(maxY, other.maxY)
    
    let newWidth = newMaxX - newMinX
    let newHeight = newMaxY - newMinY
    
    guard newWidth > 0 && newHeight > 0 else { return .zero }
    
    return IntRect(x: newMinX, y: newMinY, width: newWidth, height: newHeight)
  }
}

struct IntPoint<T: FixedWidthInteger & Comparable> {
  var x: T
  var y: T
  
  init(x: T = 0, y: T = 0) {
    self.x = x
    self.y = y
  }
  
  static var zero: IntPoint { IntPoint() }
}

struct IntSize<T: FixedWidthInteger & Comparable> {
  var width: T
  var height: T
  
  init(width: T = 0, height: T = 0) {
    self.width = width
    self.height = height
  }
  
  static var zero: IntSize { IntSize() }
}