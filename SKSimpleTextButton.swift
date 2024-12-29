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
  private var m_fPressed: Bool = false
  private static let s_knptsMarginsHorz: CGFloat = 10.0
  private static let s_knptsMarginsVert: CGFloat = 2.0
  private static let s_knptsMinSizeText: CGFloat = 8.0

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
    m_fPressed = true
    alpha = 0.7
  }

  override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
    guard let touch = touches.first else { return }
    let touchPoint = touch.location(in: self)

    if !bounds.contains(touchPoint) && m_fPressed {
      m_fPressed = false
      alpha = 1.0
    }
  }

  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    guard let touch = touches.first else { return }
    let touchPoint = touch.location(in: self)

    if bounds.contains(touchPoint) && m_fPressed {
      m_closureAction(self)
    }
    m_fPressed = false
    alpha = 1.0
  }

  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    m_fPressed = false
    alpha = 1.0
  }
  func setPosAndSize(position: CGPoint, size: CGSize) {
    self.position = position
    let fNeedsUpdate = (self.size != size)
    if fNeedsUpdate {
      self.size = size
      m_closureOnSizeChange(self)
      _updateLayout()
    }
  }
  private func _updateLayout() {
    // First try with margins
    m_labelNode.fontSize = size.height
    let bounds = m_labelNode.calculateAccumulatedFrame()

    let scaleWithMarginsH = (size.width - (2 * Self.s_knptsMarginsHorz)) / bounds.width
    let scaleWithMarginsV = size.height / bounds.height
    let scaleWithMargins = min(scaleWithMarginsH, scaleWithMarginsV)

    let fontSizeWithMargins = m_labelNode.fontSize * scaleWithMargins

    if fontSizeWithMargins >= Self.s_knptsMinSizeText {
      m_labelNode.fontSize = fontSizeWithMargins
    } else {
      // Use full space without margins
      m_labelNode.fontSize = size.height
      let boundsNoMargins = m_labelNode.calculateAccumulatedFrame()
      let scaleNoMargins = min(
        size.width / boundsNoMargins.width,
        size.height / boundsNoMargins.height)
      m_labelNode.fontSize *= scaleNoMargins
    }

    m_labelNode.position = CGPoint(x: size.width / 2, y: size.height / 2)
  }

}
