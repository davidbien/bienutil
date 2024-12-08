// VerboseLog.swift
// dbien
// 21APR2024
// Verbose logging utility.

import Foundation
import os.log

class VerboseLogManager {
  static let shared = VerboseLogManager()

  #if DEBUG
    private var isVerboseEnabled: Bool = true
  #else
    private var isVerboseEnabled: Bool = false
  #endif

  private init() {}

  func enableVerbose(_ enable: Bool) {
    isVerboseEnabled = enable
  }

  func isEnabled() -> Bool {
    return isVerboseEnabled
  }
}

func VerboseLog(_ message: String, file: String = #file, line: Int = #line, function: String = #function) {
  if VerboseLogManager.shared.isEnabled() {
    let fileName = (file as NSString).lastPathComponent
    os_log(
      "[VERBOSE] %{public}@:%d %{public}@: %{public}@",
      log: OSLog.default,
      type: .debug,
      fileName,
      line,
      function,
      message)
  }
}
