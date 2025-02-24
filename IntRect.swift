// IntRect.swift
// Integer-based rectangle template.
// dbien
// 13JAN2025

struct IntRect<T: FixedWidthInteger & Comparable>: Equatable {
  var m_ptOrigin: IntPoint<T>
  var m_szSize: IntSize<T>

  init(origin: IntPoint<T> = .zero, size: IntSize<T> = .zero) {
    m_ptOrigin = origin
    m_szSize = size
  }

  init(x: T, y: T, width: T, height: T) {
    m_ptOrigin = IntPoint(x: x, y: y)
    m_szSize = IntSize(width: width, height: height)
  }
  mutating func set(x: T, y: T, width: T, height: T) {
    m_ptOrigin = IntPoint(x: x, y: y)
    m_szSize = IntSize(width: width, height: height)
  }

  static var zero: IntRect { IntRect(origin: .zero, size: .zero) }

  var minX: T {
    get { m_ptOrigin.x }
    set { m_ptOrigin.x = newValue }
  }
  var minY: T {
    get { m_ptOrigin.y }
    set { m_ptOrigin.y = newValue }
  }
  var maxX: T { m_ptOrigin.x + m_szSize.width }
  var maxY: T { m_ptOrigin.y + m_szSize.height }
  var width: T {
    get { m_szSize.width }
    set { m_szSize.width = newValue }
  }
  var height: T {
    get { m_szSize.height }
    set { m_szSize.height = newValue }
  }
  var origin: IntPoint<T> {
    get { m_ptOrigin }
    set { m_ptOrigin = newValue }
  }
  var size: IntSize<T> {
    get { m_szSize }
    set { m_szSize = newValue }
  }

  var isEmpty: Bool { m_szSize.width <= 0 || m_szSize.height <= 0 }
  var fScanline: Bool { m_szSize.height == 1 }

  func outsetBy(dx: T, dy: T) -> IntRect {
    IntRect(x: minX - dx, y: minY - dy, width: width + dx + dx, height: height + dy + dy)
  }
  func insetBy(dx: T, dy: T) -> IntRect {
    IntRect(x: minX + dx, y: minY + dy, width: width - dx - dx, height: height - dy - dy)
  }
  func scaleBy(numerator: T, denominator: T) -> IntRect<T> {
    return IntRect(
      origin: m_ptOrigin.scaleBy(numerator: numerator, denominator: denominator),
      size: m_szSize.scaleBy(numerator: numerator, denominator: denominator)
    )
  }

  func intersects(_ rectOther: IntRect) -> Bool {
    guard !isEmpty && !rectOther.isEmpty else { return false }
    return maxX > rectOther.minX && minX < rectOther.maxX && maxY > rectOther.minY
      && minY < rectOther.maxY
  }
  func intersection(_ rectOther: IntRect) -> IntRect {
    let nMinX = Swift.max(minX, rectOther.minX)
    let nMinY = Swift.max(minY, rectOther.minY)
    let nMaxX = Swift.min(maxX, rectOther.maxX)
    let nMaxY = Swift.min(maxY, rectOther.maxY)
    let nWidth = nMaxX - nMinX
    let nHeight = nMaxY - nMinY
    guard nWidth > 0 && nHeight > 0 else { return .zero }
    return IntRect(x: nMinX, y: nMinY, width: nWidth, height: nHeight)
  }
  func contains(_ ptTest: IntPoint<T>) -> Bool {
    ptTest.x >= minX && ptTest.x < maxX && ptTest.y >= minY && ptTest.y < maxY
  }
  func contains(x: T, y: T) -> Bool {
    x >= minX && x < maxX && y >= minY && y < maxY
  }

  func convertToScanlines() -> IntRegion<T> {
    let rgnScanlines = IntRegion<T>()
    for nY in minY ..< maxY {
      rgnScanlines.add(IntRect(x: minX, y: nY, width: width, height: T(1)))
    }
    return rgnScanlines
  }

  func mergeScanline(_ rectOther: IntRect<T>) -> IntRect<T> {
    bienAssert(
      fScanline && rectOther.fScanline && minY == rectOther.minY,
      "Can only merge scanlines at same Y coordinate")
    let nMinX = Swift.min(minX, rectOther.minX)
    let nMaxX = Swift.max(maxX, rectOther.maxX)
    return IntRect(x: nMinX, y: minY, width: nMaxX - nMinX, height: T(1))
  }

  // Added method to count how many cells lie on the border (perimeter)
  func borderCellsCount() -> T {
    if width <= 0 || height <= 0 { return 0 }
    if width == 1 || height == 1 {
      return width * height
    }
    return 2 * (width + height - T(2))
  }
}

struct IntPoint<T: FixedWidthInteger & Comparable>: Equatable {
  static var zero: IntPoint { IntPoint() }
  private var m_x: T
  private var m_y: T

  init(x: T = 0, y: T = 0) {
    m_x = x
    m_y = y
  }
  var x: T {
    get { m_x }
    set { m_x = newValue }
  }
  var y: T {
    get { m_y }
    set { m_y = newValue }
  }
  func scaleBy(numerator: T, denominator: T) -> IntPoint<T> {
    return IntPoint(
      x: (m_x * numerator) / denominator,
      y: (m_y * numerator) / denominator
    )
  }
}

struct IntSize<T: FixedWidthInteger & Comparable>: Equatable {
  static var zero: IntSize { IntSize() }
  private var m_dx: T
  private var m_dy: T

  init(width: T = 0, height: T = 0) {
    m_dx = width
    m_dy = height
  }
  var width: T {
    get { m_dx }
    set { m_dx = newValue }
  }
  var height: T {
    get { m_dy }
    set { m_dy = newValue }
  }
  func scaleBy(numerator: T, denominator: T) -> IntSize<T> {
    return IntSize(
      width: ((m_dx * numerator) + (denominator - 1)) / denominator,
      height: ((m_dy * numerator) + (denominator - 1)) / denominator
    )
  }
}
