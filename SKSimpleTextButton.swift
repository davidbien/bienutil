// SKSimpleTextButton.swift
// dbien
// 21APR2024
// A simple text-based button implementation for SpriteKit.

import SpriteKit

class SKSimpleTextButton: SKSpriteNode {
  private var m_labelNode: SKLabelNode
  private var m_closureAction: (SKSpriteNode) -> Void
  private var m_closureOnRenderSchemeChange: (SKSimpleTextButton) -> Void
  private var m_closureOnPositionChange: (SKSimpleTextButton) -> Void

  init(
    text: String, fontName: String = "Helvetica", fontSize: CGFloat = 32,
    buttonColor: UIColor = .blue, size: CGSize, action: @escaping (SKSpriteNode) -> Void
  ) {
    m_labelNode = SKLabelNode(fontNamed: fontName)
    m_labelNode.text = text
    m_labelNode.fontSize = fontSize
    m_labelNode.fontColor = .white
    m_closureAction = action

    super.init(texture: nil, color: buttonColor, size: size)

    isUserInteractionEnabled = true
    m_labelNode.verticalAlignmentMode = .center
    m_labelNode.horizontalAlignmentMode = .center
    addChild(m_labelNode)
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) not supported")
  }

  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
    alpha = 0.7
  }

  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    alpha = 1.0
    m_closureAction()
  }

  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    alpha = 1.0
  }

  func setPosAndSize(position: CGPoint, size: CGSize) {
    self.position = position
    let fNeedsUpdate = (self.size != size)
    if fNeedsUpdate {
      self.size = size
      _updateLayout()
    }
  }
  // this method is called by the hierarchy when the color scheme changes - or font, etc.
  func onRenderSchemeChange() {
    _updateLayout()
  }
  private func _updateLayout() {

  }
}
