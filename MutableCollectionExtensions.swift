
// MutableCollectionExtensions.swift
// dbien
// 15JAN2025
// Extensions to MutableCollection

extension MutableCollection {
    // Partition the collection by a predicate.
    // Returns the index of the first element that does not satisfy the predicate after the partitioning.
    @discardableResult
    mutating func partition(by predicate: (Element) -> Bool) -> Index {
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
}