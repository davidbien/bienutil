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

let s_colorMap: [String: UIColor] = [
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
  return s_colorMap[colorString.lowercased()] ?? .black
}

func UIColorToJSString(_ color: UIColor) -> String {
  // First try to find a matching system color
  for (name, systemColor) in s_colorMap {
    if color == systemColor {
      return name
    }
  }

  // If no match, convert to #RRGGBBAA format
  let colorInRGBSpace = color.cgColor.converted(
    to: CGColorSpace(name: CGColorSpace.sRGB)!, intent: .defaultIntent, options: nil)!
  let components = colorInRGBSpace.components!

  return String(
    format: "#%02X%02X%02X%02X",
    Int(components[0] * 255),
    Int(components[1] * 255),
    Int(components[2] * 255),
    Int(components[3] * 255))
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

public func ShowModalAlert(title: String, message: String) {
  let alert = UIAlertController(
    title: NSLocalizedString(title, comment: ""),
    message: NSLocalizedString(message, comment: ""),
    preferredStyle: .alert)
  alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: ""), style: .default))
  if let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
    let rootVC = windowScene.windows.first?.rootViewController
  {
    rootVC.present(alert, animated: true)
  }
}

/*
ContrastRatio calculates the WCAG 2.0 contrast ratio between two colors
Returns a value between 1 and 21, where:
- 1:1 = no contrast (same colors)
- 21:1 = maximum contrast (black/white)

Minimum ratios for readability:
- 3:1 - Large text (18pt+)
- 4.5:1 - Normal text
- 7:1 - Enhanced contrast

Example usage:
let ratio = ContrastRatio(between: .black, and: .purple)
let isReadable = ratio >= 4.5

RelativeLuminance calculates perceived brightness of a color
using RGB coefficients based on human perception:
- Red: 21.26%
- Green: 71.52%
- Blue: 7.22%
*/
func ContrastRatio(between textColor: UIColor, and backgroundColor: UIColor) -> CGFloat {
  let textLuminance = RelativeLuminance(of: textColor)
  let backgroundLuminance = RelativeLuminance(of: backgroundColor)

  let lighter = max(textLuminance, backgroundLuminance)
  let darker = min(textLuminance, backgroundLuminance)

  return (lighter + 0.05) / (darker + 0.05)
}
private func RelativeLuminance(of color: UIColor) -> CGFloat {
  var red: CGFloat = 0
  var green: CGFloat = 0
  var blue: CGFloat = 0
  color.getRed(&red, green: &green, blue: &blue, alpha: nil)

  let rLum = (red <= 0.03928) ? red / 12.92 : pow((red + 0.055) / 1.055, 2.4)
  let gLum = (green <= 0.03928) ? green / 12.92 : pow((green + 0.055) / 1.055, 2.4)
  let bLum = (blue <= 0.03928) ? blue / 12.92 : pow((blue + 0.055) / 1.055, 2.4)

  return 0.2126 * rLum + 0.7152 * gLum + 0.0722 * bLum
}

enum EReadabilityLevel {
  case largeText  // 3:1 minimum
  case normalText  // 4.5:1 minimum
  case enhancedText  // 7:1 minimum
}
func IsTextReadable(textColor: UIColor, backgroundColor: UIColor, level: EReadabilityLevel) -> Bool
{
  let ratio = ContrastRatio(between: textColor, and: backgroundColor)

  switch level {
  case .largeText:
    return ratio >= 3.0
  case .normalText:
    return ratio >= 4.5
  case .enhancedText:
    return ratio >= 7.0
  }
}

func MaxContrastColor(for backgroundColor: UIColor) -> UIColor {
  let luminance = RelativeLuminance(of: backgroundColor)
  return luminance > 0.5 ? .black : .white
}

func AdjustColorForReadability(
  color: UIColor, against backgroundColor: UIColor, level: EReadabilityLevel
) -> UIColor {
  var h: CGFloat = 0
  var s: CGFloat = 0
  var b: CGFloat = 0
  var a: CGFloat = 0
  color.getHue(&h, saturation: &s, brightness: &b, alpha: &a)
  let targetRatio: CGFloat = level == .largeText ? 3.0 : (level == .normalText ? 4.5 : 7.0)
  let currentRatio = ContrastRatio(between: color, and: backgroundColor)
  if currentRatio >= targetRatio {
    return color
  }
  let backgroundLuminance = RelativeLuminance(of: backgroundColor)
  let needsDarkening = backgroundLuminance > 0.5
  while ContrastRatio(
    between: UIColor(hue: h, saturation: s, brightness: b, alpha: a), and: backgroundColor)
    < targetRatio
  {
    b = needsDarkening ? max(0, b - 0.05) : min(1, b + 0.05)
  }
  return UIColor(hue: h, saturation: s, brightness: b, alpha: a)
}

extension UIColor {
  func darkened(by percentage: CGFloat) -> UIColor {
    var h: CGFloat = 0
    var s: CGFloat = 0
    var b: CGFloat = 0
    var a: CGFloat = 0
    getHue(&h, saturation: &s, brightness: &b, alpha: &a)
    return UIColor(hue: h, saturation: s, brightness: b * (1 - percentage), alpha: a)
  }
}

// Randomly permute array via successive random swaps
public func Permute<T>(_ array: inout [T], iterations: Int? = nil) {
  let n = array.count
  let numIterations = iterations ?? max(100, n)

  for _ in 0 ..< numIterations {
    // Get two random indices
    let i = Int(drand48() * Double(n))
    let j = Int(drand48() * Double(n))

    // Swap elements
    let temp = array[i]
    array[i] = array[j]
    array[j] = temp
  }
}

// Non-mutating variant that returns a new array
public func Permuted<T>(_ array: [T], iterations: Int? = nil) -> [T] {
  var copy = array
  Permute(&copy, iterations: iterations)
  return copy
}

// Randomly permute array via successive random value pairs passed to closure
public func PermuteWithClosure<T>(
  _ array: inout [T], iterations: Int? = nil, exchange: (inout T, inout T) -> Void
) {
  let n = array.count
  guard n > 1 else { return }  // Nothing to permute with 0 or 1 elements

  let numIterations = iterations ?? max(100, n)

  for _ in 0 ..< numIterations {
    var i: Int
    var j: Int
    repeat {
      i = Int(drand48() * Double(n))
      j = Int(drand48() * Double(n))
    } while i == j

    array.withUnsafeMutableBufferPointer { buffer in
      exchange(&buffer[i], &buffer[j])
    }
  }
}
