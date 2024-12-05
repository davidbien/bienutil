// iOS_Util.Swift
// dbien
// 03DEC2024
// iOS utilities for Swift.

import Foundation
import UIKit

// ShareJsonFiles: This shares all json files located in a given directory. THe user can then choose how to share them.
func ShareJsonFiles(fromDirectory logDir: String) {
    let fileURLs = try? FileManager.default.contentsOfDirectory(
        at: URL(fileURLWithPath: logDir),
        includingPropertiesForKeys: nil
    ).filter { $0.pathExtension == "json" }
    
    guard let urls = fileURLs, !urls.isEmpty else { return }
    
    let activityVC = UIActivityViewController(
        activityItems: urls,
        applicationActivities: nil
    )
    
    if let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
       let rootVC = windowScene.windows.first?.rootViewController {
        rootVC.present(activityVC, animated: true)
    }
}
