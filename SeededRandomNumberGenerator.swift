// SeededRandomNumberGenerator.swift
// Implements a seeded random number generator in DEBUG builds,
// but uses the system random number generator in release builds.
// dbien
// 10JAN2025

import Foundation
import GameplayKit

struct SeededRandomNumberGenerator: RandomNumberGenerator {
  #if DEBUG
    private static let threadLocal = ThreadSpecific<SeededRandomNumberGenerator>()
    static var _shared: SeededRandomNumberGenerator {
      threadLocal.current ?? {
        let rng = SeededRandomNumberGenerator()
        threadLocal.current = rng
        return rng
      }()
    }
    static var shared = SeededRandomNumberGenerator._shared
    
    private var m_gkrng: GKMersenneTwisterRandomSource
    
    private init(seed: UInt64 = 123456789) {
      self.m_gkrng = GKMersenneTwisterRandomSource(seed: seed)
    }
    mutating func next() -> UInt64 {
      let value = UInt64(bitPattern: Int64(m_gkrng.nextInt()))
      return value
    }
    mutating func nextUniform() -> Double {
      return Double( m_gkrng.nextUniform() )
    }
    // Don't return one to rid a boundary condition for nextUniform().
    mutating func nextUniformNotOne() -> Double {
      let d = Double( nextUniform() )
      return d >= 0.9999999 ? 0.9999999 : d
    }
    mutating func setSeed(_ seed: UInt64) {
      m_gkrng = GKMersenneTwisterRandomSource(seed: seed)
    }
  #else
    static var shared = SeededRandomNumberGenerator()
    private var m_rng = SystemRandomNumberGenerator()
    private init() {}
    mutating func next() -> UInt64 {
      return m_rng.next()
    }
    mutating func nextUniform() -> Double {
      return Double(next()) / Double(UInt64.max)
    }
    mutating func nextUniformNotOne() -> Double {
      let d = Double( nextUniform() )
      return d >= 0.9999999 ? 0.9999999 : d
    }
    mutating func setSeed(_ seed: UInt64) {
      // Ignored in release builds
    }
  #endif
}
