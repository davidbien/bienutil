// SKSimpleTextButton.swift
// dbien
// 21APR2024
// Simple checkbox/radio button implementation for SpriteKit.

import SpriteKit

class SKSimpleCheckboxRadio: SKSpriteNode, RenderLocaleAware {
    private var m_closureAction: (SKSimpleCheckboxRadio) -> Void
    private var m_closureOnRenderSchemeChange: (SKSimpleCheckboxRadio) -> Void
    private var m_closureOnSizeChange: (SKSimpleCheckboxRadio) -> Void
    private var m_strLocalizationKey: String
    private var m_labelNode: SKLabelNode
    private var m_shCheckbox: SKShapeNode
    private var m_shInnerMark: SKShapeNode
    private var m_fIsRadio: Bool
    private var m_fIsChecked: Bool
    
    init(
        keyLocale _localizationKey: String,
        isRadio _isRadio: Bool,
        isChecked _isChecked: Bool = false,
        action _action: @escaping (SKSimpleCheckboxRadio) -> Void,
        schemeChange _onRenderSchemeChange: @escaping (SKSimpleCheckboxRadio) -> Void,
        sizeChange _onSizeChange: @escaping (SKSimpleCheckboxRadio) -> Void
    ) {
        m_strLocalizationKey = _localizationKey
        m_labelNode = SKLabelNode()
        m_shCheckbox = SKShapeNode()
        m_shInnerMark = SKShapeNode()
        m_fIsRadio = _isRadio
        m_fIsChecked = _isChecked
        m_closureAction = _action
        m_closureOnRenderSchemeChange = _onRenderSchemeChange
        m_closureOnSizeChange = _onSizeChange

        super.init(texture: nil, color: .clear, size: CGSize(width: 1, height: 1))
        self.anchorPoint = CGPoint(x: 0, y: 0)
        
        isUserInteractionEnabled = true
        m_labelNode.verticalAlignmentMode = .center
        m_labelNode.horizontalAlignmentMode = .left
        
        addChild(m_shCheckbox)
        m_shCheckbox.addChild(m_shInnerMark)
        addChild(m_labelNode)
        
        onLocaleChange()
        m_closureOnRenderSchemeChange(self)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) not supported")
    }
    
    var isChecked: Bool {
        get { m_fIsChecked }
        set {
            m_fIsChecked = newValue
            _updateInnerMark()
        }
    }
    
    var keyLocalization: String {
        get { m_strLocalizationKey }
        set {
            m_strLocalizationKey = newValue
            m_labelNode.text = NSLocalizedString(newValue, comment: "")
        }
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
        m_fIsChecked = !m_fIsChecked
        _updateInnerMark()
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
            let boxSize = min(size.height * 0.8, size.width * 0.2)
            
            m_shCheckbox.position = CGPoint(x: boxSize/2, y: size.height/2)
            if m_fIsRadio {
                m_shCheckbox.path = CGPath(ellipseIn: CGRect(x: -boxSize/2, y: -boxSize/2, width: boxSize, height: boxSize), transform: nil)
            } else {
                m_shCheckbox.path = CGPath(rect: CGRect(x: -boxSize/2, y: -boxSize/2, width: boxSize, height: boxSize), transform: nil)
            }
            
            m_labelNode.position = CGPoint(x: boxSize * 1.5, y: size.height/2)
            m_labelNode.fontSize = size.height * 0.6
            
            m_closureOnSizeChange(self)
            _updateLayout()
            _updateInnerMark()
        }
    }
    
    private func _updateInnerMark() {
        guard let boxPath = m_shCheckbox.path else { return }
        let bounds = boxPath.boundingBox
        let innerSize = bounds.width * 0.6
        
        if m_fIsRadio {
            m_shInnerMark.path = CGPath(ellipseIn: CGRect(x: -innerSize/2, y: -innerSize/2, width: innerSize, height: innerSize), transform: nil)
        } else {
            let checkPath = CGMutablePath()
            let scale = innerSize/10
            checkPath.move(to: CGPoint(x: -2 * scale, y: 0))
            checkPath.addLine(to: CGPoint(x: -1 * scale, y: -2 * scale))
            checkPath.addLine(to: CGPoint(x: 4 * scale, y: 2 * scale))
            m_shInnerMark.path = checkPath
        }
        m_shInnerMark.isHidden = !m_fIsChecked
    }
    
    private func _updateLayout() {
        m_shCheckbox.strokeColor = .white
        m_shCheckbox.lineWidth = 2.0
        m_shInnerMark.strokeColor = .white
        m_shInnerMark.lineWidth = 2.0
        m_shInnerMark.fillColor = .white
    }
}