
// OptionSetCodable.swift
// dbien
// 27JAN2025
// A protocol that allows OptionSets to be encoded and decoded using Codable.

protocol OptionSetCodable: OptionSet, Codable where RawValue: FixedWidthInteger & Codable {
    // Empty protocol - just combines the requirements
}

extension OptionSetCodable {
    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let rawValue = try container.decode(RawValue.self)
        self.init(rawValue: rawValue)
    }
    
    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(rawValue)
    }
}

protocol OptionSetCodableNames: OptionSet where RawValue: FixedWidthInteger {
    static var nameToValue: [String: RawValue] { get }
    static var valueToName: [RawValue: String] { get }
}

extension OptionSetCodableNames {
    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let names = try container.decode([String].self)
        let rawValue = try names.reduce(RawValue(0)) { result, name in
            guard let value = Self.nameToValue[name] else {
                throw DecodingError.dataCorruptedError(
                    in: container,
                    debugDescription: "Unknown option name: \(name)"
                )
            }
            return result | value
        }
        self.init(rawValue: rawValue)
    }
    
    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        var names: [String] = []
        for (value, name) in Self.valueToName {
            if rawValue & value == value {
                names.append(name)
            }
        }
        try container.encode(names)
    }
}
// Usage example for OptionSetCodableNames:
// struct AnimationOptions: OptionSetCodableNames, Codable {
//     let rawValue: UInt32
    
//     static let looping     = AnimationOptions(rawValue: 1 << 0)
//     static let teardown    = AnimationOptions(rawValue: 1 << 1)
//     static let removeOnEnd = AnimationOptions(rawValue: 1 << 2)
    
//     static let defaultOptions: AnimationOptions = [.looping, .teardown]
    
//     static let nameToValue: [String: UInt32] = [
//         "looping": 1 << 0,
//         "teardown": 1 << 1,
//         "removeOnEnd": 1 << 2
//     ]
    
//     static let valueToName: [UInt32: String] = [
//         1 << 0: "looping",
//         1 << 1: "teardown",
//         1 << 2: "removeOnEnd"
//     ]
// }

