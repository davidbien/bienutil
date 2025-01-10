// SeededRandomNumberGenerator.swift
// Implements a seeded random number generator in DEBUG builds,
// but uses the system random number generator in release builds.
// dbien
// 10JAN2025

import Foundation
import GameplayKit

struct SeededRandomNumberGenerator: RandomNumberGenerator {
  static var shared = SeededRandomNumberGenerator()

  #if DEBUG
    private var m_gkrng: GKMersenneTwisterRandomSource
    private var m_dist: GKRandomDistribution
    private init(seed: UInt64 = 1) {
      self.m_gkrng = GKMersenneTwisterRandomSource(seed: seed)
      self.m_dist = GKRandomDistribution(
        randomSource: m_gkrng, lowestValue: Int64.min, highestValue: Int64.max)
    }
    mutating func next() -> UInt64 {
      return UInt64(bitPattern: Int64(m_dist.nextInt()))
    }
    mutating func nextUniform() -> Double {
      return Double(m_dist.nextUniform())
    }
    mutating func setSeed(_ seed: UInt64) {
      m_gkrng = GKMersenneTwisterRandomSource(seed: seed)
      m_dist = GKRandomDistribution(
        randomSource: m_gkrng, lowestValue: Int64.min, highestValue: Int64.max)
    }
  #else
    private var m_rng = SystemRandomNumberGenerator()
    private init() {}
    mutating func next() -> UInt64 {
      return m_rng.next()
    }
    mutating func nextUniform() -> Double {
      return Double(next()) / Double(UInt64.max)
    }
    mutating func setSeed(_ seed: UInt64) {
      // Ignored in release builds
    }
  #endif
}
