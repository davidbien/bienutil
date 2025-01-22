// MutableCollectionExtensions.swift
// dbien
// 15JAN2025
// Extensions to MutableCollection

extension MutableCollection where Self: RandomAccessCollection {
  // Partition the collection by a predicate.
  // Returns the index of the first element that does not satisfy the predicate after the partitioning.
  @discardableResult
  mutating func partitionByPredicate(by predicate: (Element) -> Bool) -> Index {
    var p = startIndex
    for i in indices {
      if predicate(self[i]) {
        if i != p {
          swapAt(i, p)
        }
        p = index(after: p)
      }
    }
    return p
  }

  mutating func shuffleElements<T: RandomNumberGenerator>(using generator: inout T) {
    guard count > 1 else { return }

    for i in indices.dropLast() {
      let remainingCount = distance(from: i, to: endIndex)
      let j = index(i, offsetBy: Int(generator.next() % UInt64(remainingCount)))
      if i != j {
        swapAt(i, j)
      }
    }
  }
}
