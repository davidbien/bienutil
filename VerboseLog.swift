// VerboseLog.swift
// dbien
// 21APR2024
// Verbose logging utility.

import Foundation
import os.log

class VerboseLogManager {
  static let shared = VerboseLogManager()

  #if DEBUG || VERBOSE_LOGGING_ENABLED
    private var isVerboseEnabled: Bool = true
  #else
    private var isVerboseEnabled: Bool = true
  #endif

  private init() {}

  func enableVerbose(_ enable: Bool) {
    isVerboseEnabled = enable
  }

  func isEnabled() -> Bool {
    return isVerboseEnabled
  }
}

private let logger = OSLog(subsystem: "com.davidbien.CreatePlaylistApp", category: "verbose")

func VerboseLog(
  _ message: String, file: String = #file, line: Int = #line, function: String = #function
) {
  if VerboseLogManager.shared.isEnabled() {
    let fileName = (file as NSString).lastPathComponent
    os_log(
      "[VERBOSE] %{public}@:%d %{public}@: %{public}@",
      log: logger,
      type: .debug,
      fileName,
      line,
      function,
      message)
  }
}

class VerboseLogObject {
    private let previousState: Bool
    
    init(enable: Bool) {
        previousState = VerboseLogManager.shared.isEnabled()
        VerboseLogManager.shared.enableVerbose(enable)
    }
    
    deinit {
        VerboseLogManager.shared.enableVerbose(previousState)
    }
}
