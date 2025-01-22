// bienAssert.swift
// dbien
// 02DEC2024
// Custom assertions that can be enabled in both debug and release builds.
// Provides bienAssert() which can be configured to halt or continue on failure. bienAssert() will not execute the associated code if assertions are disabled.
// Also provides bienVerify() which always logs and continues on failure. bienVerify() will always execute the associated code, it will only log if assertions are enabled.

import Foundation
import os.log

enum AssertionControl {
  #if DEBUG
    static var areAssertionsEnabled = true
    static var continueOnFailure = true
  #else
    static var areAssertionsEnabled = false
    static var continueOnFailure = false
  #endif
}

func bienAssert(
  _ condition: @autoclosure () -> Bool,
  _ message: @autoclosure () -> String = String(),
  file: StaticString = #file, 
  line: UInt = #line,
  function: StaticString = #function
) {
  // We don't execute the code if assertions are disabled.
  if AssertionControl.areAssertionsEnabled && !condition() {
    let strFile: String = file.description
    let fileName = (strFile as NSString).lastPathComponent
    os_log(
      "[ASSERT] %{public}@ line %d: %{public}@: %{public}@",
      log: OSLog.default,
      type: .debug,
      fileName,
      UInt32(line),
      String(describing: function),
      message())
    if !AssertionControl.continueOnFailure {
      precondition(false, message(), file: file, line: line)
    }
  }
}

func bienVerify(
  _ condition: @autoclosure () -> Bool,
  _ message: @autoclosure () -> String = String(),
  file: StaticString = #file, 
  line: UInt = #line,
  function: StaticString = #function
) {
  // We always execute a verify statement, we only log if assertions are enabled.
  if !condition() && AssertionControl.areAssertionsEnabled {
    let strFile: String = file.description
    let fileName = (strFile as NSString).lastPathComponent
    os_log(
      "[VERIFY] %{public}@ line %d: %{public}@: %{public}@",
      log: OSLog.default,
      type: .debug,
      fileName,
      UInt32(line),
      String(describing: function),
      message())
  }
}
