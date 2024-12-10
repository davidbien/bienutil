// RenderLocaleAware.swift
// dbien
// 17APR2024
// Protocol for objects that want to react to color or locale changes.

protocol RenderLocaleAware {
    func onRenderSchemeChange()
    func onLocaleChange()
}

// object can implement as many of these as they want.
extension RenderLocaleAware {
    func onRenderSchemeChange() {
        // Default implementation
    }
    func onLocaleChange() {
        // Default implementation
    }
}