// ColorCodable.swift
// dbien
// 29DEC2024
// A simple struct to encode and decode a UIColor that conforms to the Codable protocol.

import UIKit

struct ColorCodable: Codable {
  let uiColor: UIColor

  init(_ color: UIColor) {
    self.uiColor = color
  }

  init(from decoder: Decoder) throws {
    let container = try decoder.singleValueContainer()
    let colorString = try container.decode(String.self)
    self.uiColor = UIColorFromJSString(colorString)
  }

  func encode(to encoder: Encoder) throws {
    var container = encoder.singleValueContainer()
    try container.encode(UIColorToJSString(uiColor))
  }
}
extension ColorCodable: ExpressibleByStringLiteral {
    init(stringLiteral value: String) {
        self.uiColor = UIColorFromJSString(value)
    }
}