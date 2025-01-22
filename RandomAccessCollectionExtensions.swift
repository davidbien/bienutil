// RandomAccessCollectionExtensions.swift
// dbien
// 03DEC2024
// Extensions for RandomAccessCollection.

/// Usage examples:
///
/// Binary search on sorted array using lowerBound with comparator:
/// ```
/// let numbers = [1, 2, 2, 2, 3, 4, 5]
/// let start = numbers.lowerBound(value: 2) { $0 < $1 }  // First element not less than 2
/// ```
///
/// Binary search using upperBound with comparator:
/// ```
/// let end = numbers.upperBound(value: 2) { $0 < $1 }    // First element greater than 2
/// ```
///
/// Find range of equivalent elements:
/// ```
/// let range = numbers.equalRange(value: 2) { $0 < $1 }  // Range containing all 2s
/// ```
///
/// Note: Collection must be sorted according to comparator's ordering.

extension RandomAccessCollection {
  func lowerBound(value: Element, comp: (Element, Element) -> Bool) -> Index {
    var low: Self.Index = startIndex
    var high: Self.Index = endIndex
    while low != high {
      let mid: Self.Index = index(low, offsetBy: distance(from: low, to: high) / 2)
      if comp(self[mid], value) {
        low = index(after: mid)
      } else {
        high = mid
      }
    }
    return low
  }

  func upperBound(value: Element, comp: (Element, Element) -> Bool) -> Index {
    var low: Self.Index = startIndex
    var high: Self.Index = endIndex
    while low != high {
      let mid: Self.Index = index(low, offsetBy: distance(from: low, to: high) / 2)
      if !comp(value, self[mid]) {
        low = index(after: mid)
      } else {
        high = mid
      }
    }
    return low
  }

  func equalRange(value: Element, comp: (Element, Element) -> Bool) -> Range<Index> {
    return lowerBound(value: value, comp: comp) ..< upperBound(value: value, comp: comp)
  }

  func getRandomElement<T: RandomNumberGenerator>(using generator: inout T) -> Element? {
    guard !isEmpty else { return nil }
    let count = distance(from: startIndex, to: endIndex)
    let randomOffset = Int(generator.next() % UInt64(count))
    return self[index(startIndex, offsetBy: randomOffset)]
  }
}
