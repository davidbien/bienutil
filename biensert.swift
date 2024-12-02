// biensert.swift
// dbien
// 02DEC2024
// Custom assertions that can be enabled in both debug and release builds.
// Provides biensert() which can be configured to halt or continue on failure.
// Also provides biensertContinue() which always logs and continues on failure.

import Foundation

enum AssertionControl {
    #if DEBUG
    static var areAssertionsEnabled = true
    static var continueOnFailure = false
    #else
    static var areAssertionsEnabled = false
    static var continueOnFailure = false
    #endif
}

func biensert(_ condition: @autoclosure () -> Bool, 
              _ message: @autoclosure () -> String = String(),
              file: StaticString = #file,
              line: UInt = #line) {
    if AssertionControl.areAssertionsEnabled && !condition() {
        if AssertionControl.continueOnFailure {
            NSLog("Assertion failed(continuing execution): \(message()) - \(file):\(line)")
        } else {
            precondition(false, message(), file: file, line: line)
        }
    }
}

func biensertContinue(_ condition: @autoclosure () -> Bool, 
                      _ message: @autoclosure () -> String = String(),
                      file: StaticString = #file,
                      line: UInt = #line) {
    if AssertionControl.areAssertionsEnabled && !condition() {
        NSLog("Assertion failed(continuing execution): \(message()) - \(file):\(line)")
    }
}