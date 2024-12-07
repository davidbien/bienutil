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
    let rootVC = windowScene.windows.first?.rootViewController
  {
    rootVC.present(activityVC, animated: true)
  }
}

// Translate a string that is in the format "#RRGGBBAA", "#RRGGBB", or a color name to a UIColor
func UIColorFromJSString(_ colorString: String) -> UIColor {
  if colorString.hasPrefix("#") {
    let hex = String(colorString.dropFirst())
    var rgbValue: UInt64 = 0
    if !Scanner(string: hex).scanHexInt64(&rgbValue) {
      NSLog("Invalid color string format: \(colorString)")
      return .black
    }

    let length = hex.count
    if length == 8 {  // #RRGGBBAA
      return UIColor(
        red: CGFloat((rgbValue >> 24) & 0xFF) / 255.0,
        green: CGFloat((rgbValue >> 16) & 0xFF) / 255.0,
        blue: CGFloat((rgbValue >> 8) & 0xFF) / 255.0,
        alpha: CGFloat(rgbValue & 0xFF) / 255.0)
    } else if length == 6 {  // #RRGGBB
      return UIColor(
        red: CGFloat((rgbValue >> 16) & 0xFF) / 255.0,
        green: CGFloat((rgbValue >> 8) & 0xFF) / 255.0,
        blue: CGFloat(rgbValue & 0xFF) / 255.0,
        alpha: 1.0)
    } else {
      NSLog("Invalid color string format: \(colorString)")
      return .black
    }
  }

  // Handle named colors
  return UIColor(named: colorString) ?? .black
}

// Creates a path that draws 4 lines whose outer edges exactly match the input rectangle
// this eliminates the annoying background showing through a couple of pixels at the very corner
// when just using a rectangle - results in perfectly square corners with any line thickness.
func CreateSquarePathFromRectInner(rect: CGRect, thicknessX: CGFloat, thicknessY: CGFloat = -1) -> CGPath {
    let halfThickX = thicknessX / 2
    let halfThickY = thicknessY < 0 ? halfThickX : thicknessY / 2
    let path = CGMutablePath()
    path.move(to: CGPoint(x: rect.minX + halfThickX, y: rect.maxY - halfThickY))
    path.addLine(to: CGPoint(x: rect.maxX - halfThickX, y: rect.maxY - halfThickY))
    path.move(to: CGPoint(x: rect.maxX - halfThickX, y: rect.maxY - halfThickX))
    path.addLine(to: CGPoint(x: rect.maxX - halfThickX, y: rect.minY + halfThickX))
    path.move(to: CGPoint(x: rect.maxX - halfThickX, y: rect.minY + halfThickY))
    path.addLine(to: CGPoint(x: rect.minX + halfThickX, y: rect.minY + halfThickY))
    path.move(to: CGPoint(x: rect.minX + halfThickX, y: rect.minY + halfThickX))
    path.addLine(to: CGPoint(x: rect.minX + halfThickX, y: rect.maxY - halfThickX))
    return path
}
