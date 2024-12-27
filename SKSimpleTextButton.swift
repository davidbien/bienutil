// SKSimpleTextButton.swift
// dbien
// 21APR2024
// A simple text-based button implementation for SpriteKit.

import SpriteKit

class SKSimpleTextButton: SKSpriteNode, RenderLocaleAware {
  private var m_closureAction: (SKSimpleTextButton) -> Void = { _ in }
  private var m_closureOnRenderSchemeChange: (SKSimpleTextButton) -> Void = { _ in }
  private var m_closureOnSizeChange: (SKSimpleTextButton) -> Void = { _ in }
  private var m_strLocalizationKey: String = ""
  private var m_labelNode: SKLabelNode = SKLabelNode()

  init(
    keyLocale _localizationKey: String,
    action _action: @escaping (SKSimpleTextButton) -> Void,
    schemeChange _onRenderSchemeChange: @escaping (SKSimpleTextButton) -> Void,
    sizeChange _onSizeChange: @escaping (SKSimpleTextButton) -> Void
  ) {
    m_strLocalizationKey = _localizationKey
    m_closureAction = _action
    m_closureOnRenderSchemeChange = _onRenderSchemeChange
    m_closureOnSizeChange = _onSizeChange

    super.init(texture: nil, color: .clear, size: CGSize(width: 1, height: 1))
    self.anchorPoint = CGPoint(x: 0, y: 0)
    self.colorBlendFactor = 1.0

    isUserInteractionEnabled = true
    m_labelNode.verticalAlignmentMode = .center
    m_labelNode.horizontalAlignmentMode = .center
    addChild(m_labelNode)
    onLocaleChange()
    m_closureOnRenderSchemeChange(self)  // don't update layout until we have a size.
  }
  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) not supported")
  }
  func setActionClosure(_ closure: @escaping (SKSimpleTextButton) -> Void) {
    m_closureAction = closure
  }

  var keyLocalization: String {
    get { m_strLocalizationKey }
    set {
      m_strLocalizationKey = newValue
      m_labelNode.text = NSLocalizedString(newValue, comment: "")
    }
  }
  var labelNode: SKLabelNode {
    m_labelNode
  }
  func onRenderSchemeChange() {
    m_closureOnRenderSchemeChange(self)
    _updateLayout()
  }
  func onLocaleChange() {
    m_labelNode.text = NSLocalizedString(m_strLocalizationKey, comment: "")
  }
  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
    alpha = 0.7
  }
  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    alpha = 1.0
    m_closureAction(self)
  }
  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    alpha = 1.0
  }
  func setPosAndSize(position: CGPoint, size: CGSize) {
    self.position = position
    let fNeedsUpdate = (self.size != size)
    if fNeedsUpdate {
      self.size = size
      // Set the label position to the center of the button by default the closure can change this if it wants.
      m_labelNode.position = CGPoint(x: size.width / 2, y: size.height / 2)
      // Set the label font size to the height of the button by default the closure can change this if it wants.
      m_labelNode.fontSize = size.height
      m_closureOnSizeChange(self)
      _updateLayout()
    }
  }
  private func _updateLayout() {

  }
}
