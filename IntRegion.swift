// IntRegion.swift
// Integer-based region template implemented as a set of rectangles.
// dbien
// 13JAN2025

class IntRegion<T: FixedWidthInteger & Comparable> {
  // Rectangles sorted by minY, then minX for efficient operations
  private var m_rgrect: [IntRect<T>]
  
  init() {
    m_rgrect = []
  }
  
  init(_ rect: IntRect<T>) {
    m_rgrect = rect.isEmpty ? [] : [rect]
  }
  
  init(_ rgrect: [IntRect<T>]) {
    m_rgrect = []
    for rect in rgrect.filter({ !$0.isEmpty }) {
      add(rect)
    }
  }
  
  var rects: [IntRect<T>] { m_rgrect }
  var isEmpty: Bool { m_rgrect.isEmpty }
  var bounds: IntRect<T> {
    guard !isEmpty else { return .zero }
    let nMinX = m_rgrect.map { $0.minX }.min()!
    let nMinY = m_rgrect.map { $0.minY }.min()!
    let nMaxX = m_rgrect.map { $0.maxX }.max()!
    let nMaxY = m_rgrect.map { $0.maxY }.max()!
    return IntRect(x: nMinX, y: nMinY, width: nMaxX - nMinX, height: nMaxY - nMinY)
  }  
  private func _insertRectSorted(_ rect: IntRect<T>) {
    let nIdx = m_rgrect.lowerBound(value: rect) { 
      $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) 
    }
    m_rgrect.insert(rect, at: nIdx)
  }
  private func _fRemoveRect(_ rect: IntRect<T>) -> Bool {
    let nIdx = m_rgrect.lowerBound(value: rect) { 
      $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) 
    }
    guard nIdx < m_rgrect.count && m_rgrect[nIdx] == rect else { return false }
    m_rgrect.remove(at: nIdx)
    return true
  }
  
  func add(_ region: IntRegion<T>) {
    for rect in region.rects {
      add(rect)
    }
  }
  func add(_ rgrect: [IntRect<T>]) {
    for rect in rgrect {
      add(rect)
    }
  }
  // Add a single rectangle to the region.
  func add(_ rect: IntRect<T>) {
    guard !rect.isEmpty else { return }
    
    // First try to add without any intersections - collect intersecting indices
    var rgidxRectIntersecting: [Int] = []
    for nIdx in 0..<m_rgrect.count {
      if m_rgrect[nIdx].intersects(rect) {
        rgidxRectIntersecting.append(nIdx)
      }
    }
    if rgidxRectIntersecting.isEmpty {
      _insertRectSorted(rect)
      return
    }
    
    // We have intersections - handle scanline conversion and merging
    var rgnUnprocessed = IntRegion<T>()  // Scanlines waiting to be processed
    let rgnAdditions = IntRegion<T>()    // New scanlines for self
    
    // Convert rect to scanlines if needed
    if !rect.fScanline {
      rgnUnprocessed = rect.convertToScanlines()
    } else {
      rgnUnprocessed.add(rect)
    }
    
    // Process each intersecting rect in reverse order
    for nIdx in rgidxRectIntersecting.reversed() {
      let rectExisting = m_rgrect[nIdx]
      if rectExisting.fScanline {
        // Remove from self as it will be merged
        m_rgrect.remove(at: nIdx)
        
        // Find and merge any scanlines at same Y
        let rngidxScanlines = rgnUnprocessed.m_rgrect.equalRange(value: rectExisting) { 
            $0.minY < $1.minY 
        }
        
        if !rngidxScanlines.isEmpty {
          var rectMerged = rectExisting
          // Process scanlines in reverse order - merge if intersecting and remove
          for nIdxScan in rngidxScanlines.reversed() {
            let rectScanline = rgnUnprocessed.m_rgrect[nIdxScan]
            // Fast X-axis intersection check since we know Y's match
            if rectMerged.maxX > rectScanline.minX && rectMerged.minX < rectScanline.maxX {
              rectMerged = rectMerged.mergeScanline(rectScanline)
              rgnUnprocessed.m_rgrect.remove(at: nIdxScan)
            }
          }
          rgnAdditions.add(rectMerged)
        } else {
          rgnAdditions.add(rectExisting)
        }
      } else {
        // Convert existing rect to scanlines and add to self's pending additions
        let rgnScanlines = rectExisting.convertToScanlines()
        rgnAdditions.add(rgnScanlines)
        m_rgrect.remove(at: nIdx)
      }
    }
    
    // Process any remaining unprocessed scanlines
    while !rgnUnprocessed.isEmpty {
      let rectAdd = rgnUnprocessed.m_rgrect.removeFirst()
      rgnAdditions.add(rectAdd)
    }
    
    // Add all new scanlines to self
    for rectAdd in rgnAdditions.rects {
      bienAssert( m_rgrect.filter { $0.intersects(rectAdd) }.isEmpty, "rectAdd intersects with existing rects - this is a bug.")
      _insertRectSorted(rectAdd)
    }
    assertValid()
  }

  func assertValid(fAllScanlines: Bool = false) {
    if AssertionControl.areAssertionsEnabled {
      // Check sorted order
      for nIdx in 1..<m_rgrect.count {
        let rectPrev = m_rgrect[nIdx - 1]
        let rectCur = m_rgrect[nIdx]
        bienAssert(rectPrev.minY < rectCur.minY || 
                   (rectPrev.minY == rectCur.minY && rectPrev.minX < rectCur.minX),
                   "Rects not in sorted order")
      }
      // Check for intersections
      for nIdx in 0..<m_rgrect.count {
        for nIdxOther in (nIdx + 1)..<m_rgrect.count {
          bienAssert(!m_rgrect[nIdx].intersects(m_rgrect[nIdxOther]),
                     "Found intersecting rects at indices \(nIdx) and \(nIdxOther)")
        }
      }
      // Check all rects are scanlines if required
      if fAllScanlines {
        for rect in m_rgrect {
          bienAssert(rect.fScanline, "Region contains non-scanline rect")
        }
      }
    }
  }
  // Convert all rects to scanlines rects - this allows use to search for points using equalRange
  func convertToScanlines() {
    if m_rgrect.isEmpty { return }
    var rgrectScanlines: [IntRect<T>] = []
    for rect in m_rgrect {
      if !rect.fScanline {
        let rgnRectScanlines = rect.convertToScanlines()
        for rectScanline in rgnRectScanlines.rects {
          _insertRectSorted(rectScanline, into: &rgrectScanlines)
        }
      } else {
        _insertRectSorted(rect, into: &rgrectScanlines)
      }
    }
    m_rgrect = rgrectScanlines
    assertValid(fAllScanlines: true)
  }
  // Helper method to insert into any array of rects maintaining sort order
  private func _insertRectSorted(_ rect: IntRect<T>, into rgrect: inout [IntRect<T>]) {
    let nIdx = rgrect.lowerBound(value: rect) { 
      $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) 
    }
    rgrect.insert(rect, at: nIdx)
  }

  func contains(x: T, y: T) -> Bool {
    assertValid(fAllScanlines: true)
    if m_rgrect.isEmpty {
      return false
    }
    let rectSearch = IntRect(x: x, y: y, width: 0, height: 0)
    let nIdx = m_rgrect.lowerBound(value: rectSearch) { 
      $0.minY < $1.minY || ($0.minY == $1.minY && $0.minX < $1.minX) 
    }
    return nIdx > 0 && m_rgrect[nIdx - 1].minY == y && x >= m_rgrect[nIdx - 1].minX && x < m_rgrect[nIdx - 1].maxX
  }
}

enum IntRegionError: Error {
  case intersectingRects
}