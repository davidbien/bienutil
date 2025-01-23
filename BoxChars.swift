
// BoxChars.swift
// dbien
// 22JAN2025
// Box characters for use in Sodorkyou

enum BoxChar: UInt32 {
    case vertical = 0x2502
    case horizontal = 0x2500
    case topLeft = 0x250C
    case topRight = 0x2510
    case bottomLeft = 0x2514
    case bottomRight = 0x2518
    
    var scalar: UnicodeScalar {
        UnicodeScalar(self.rawValue)!
    }
}
enum DoubleBoxChar: UInt32 {
    case vertical = 0x2551 // ║
    case horizontal = 0x2550 // ═
    case topLeft = 0x2554 // ╔
    case topRight = 0x2557 // ╗
    case bottomLeft = 0x255A // ╚
    case bottomRight = 0x255D // ╝
    
    var scalar: UnicodeScalar {
        UnicodeScalar(self.rawValue)!
    }
}
enum RoundBoxChar: UInt32 {
    case vertical = 0x2502    // │ (same as regular)
    case horizontal = 0x2500  // ─ (same as regular)
    case topLeft = 0x256D     // ╭
    case topRight = 0x256E    // ╮
    case bottomLeft = 0x2570  // ╰
    case bottomRight = 0x256F // ╯
    
    var scalar: UnicodeScalar {
        UnicodeScalar(self.rawValue)!
    }
}
enum HeavyBoxChar: UInt32 {
    case vertical = 0x2503    // ┃
    case horizontal = 0x2501  // ━
    case topLeft = 0x250F     // ┏
    case topRight = 0x2513    // ┓
    case bottomLeft = 0x2517  // ┗
    case bottomRight = 0x251B // ┛
    var scalar: UnicodeScalar {
        UnicodeScalar(self.rawValue)!
    }
}
enum DashedBoxChar: UInt32 {
    case vertical = 0x254E    // ╎
    case horizontal = 0x254C  // ┄
    case topLeft = 0x250C     // ┌ (usually regular corner)
    case topRight = 0x2510    // ┐
    case bottomLeft = 0x2514  // └
    case bottomRight = 0x2518 // ┘
    var scalar: UnicodeScalar {
        UnicodeScalar(self.rawValue)!
    }
}

