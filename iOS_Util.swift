// iOS_Util.Swift
// dbien
// 03DEC2024
// iOS utilities for Swift.

import Foundation
import SpriteKit
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

  let colorMap: [String: UIColor] = [
    "black": .black,
    "blue": .blue,
    "brown": .brown,
    "cyan": .cyan,
    "darkgray": .darkGray,
    "gray": .gray,
    "green": .green,
    "lightgray": .lightGray,
    "magenta": .magenta,
    "orange": .orange,
    "purple": .purple,
    "red": .red,
    "white": .white,
    "yellow": .yellow,
    "systemblue": .systemBlue,
    "systembrown": .systemBrown,
    "systemgreen": .systemGreen,
    "systemindigo": .systemIndigo,
    "systemorange": .systemOrange,
    "systempink": .systemPink,
    "systempurple": .systemPurple,
    "systemred": .systemRed,
    "systemteal": .systemTeal,
    "systemyellow": .systemYellow,
    "systemgray": .systemGray,
    "systemgray2": .systemGray2,
    "systemgray3": .systemGray3,
    "systemgray4": .systemGray4,
    "systemgray5": .systemGray5,
    "systemgray6": .systemGray6,
  ]
  return colorMap[colorString.lowercased()] ?? .black
}

// Creates a path that draws 4 lines whose outer edges exactly match the input rectangle
// this eliminates the annoying background showing through a couple of pixels at the very corner
// when just using a rectangle - results in perfectly square corners with any line thickness.
func CreateSquarePathFromRectInner(rect: CGRect, thicknessX: CGFloat, thicknessY: CGFloat = -1)
  -> CGPath
{
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

func CreateRectPathFromRectInner(rect: CGRect, thickness: CGFloat) -> CGPath {
  let halfThickness = thickness / 2
  let innerRect = rect.insetBy(dx: halfThickness, dy: halfThickness)
  let path = CGPath(rect: innerRect, transform: nil)
  return path
}

public func ConvertLength(_ scene: SKScene, _ length: CGFloat) -> CGFloat {
  let origin = scene.convertPoint(fromView: CGPoint.zero)
  let point = scene.convertPoint(fromView: CGPoint(x: length, y: 0))
  return CGFloat(floor(abs(point.x - origin.x)))
}

// Convert x and y lengths in case they end up diff.
public func ConvertLengths(_ scene: SKScene, _ length: CGFloat) -> (CGFloat, CGFloat) {
  let origin = scene.convertPoint(fromView: CGPoint.zero)
  let point = scene.convertPoint(fromView: CGPoint(x: length, y: length))
  return (CGFloat(floor(abs(point.x - origin.x))), CGFloat(floor(abs(point.y - origin.y))))
}
