// SKNodeExtensions.swift
// dbien
// 05JAN2025
// Extensions for SKNode.

import SpriteKit

extension SKNode {
  func FindParentViewController<T: UIViewController>(ofType type: T.Type) -> T? {
    var node: SKNode? = self
    while let current = node {
      if let scene = current as? SKScene {
        var parentView: UIView? = scene.view
        while let currentView = parentView {
          if let viewController = currentView.next as? T {
            return viewController
          }
          parentView = currentView.superview
        }
        break
      }
      node = current.parent
    }
    return nil
  }
  func GetTabController<T: UIViewController>(
    ofType viewControllerType: T.Type
  ) -> MainTabBarController? {
    if let viewController = FindParentViewController(ofType: viewControllerType) {
      return viewController.tabBarController as? MainTabBarController
    }
    return nil
  }
  func ApplyToParentViewController<T: UIViewController>(
    to viewControllerType: T.Type, closureApplyToParent: (T) -> Void
  ) {
    if let viewController = FindParentViewController(ofType: viewControllerType) {
      closureApplyToParent(viewController)
    }
  }
  func AdjustChildrenPositions(xOffset: CGFloat, yOffset: CGFloat) {
    for child: SKNode in children {
      child.position = CGPoint(
        x: child.position.x - xOffset,
        y: child.position.y - yOffset
      )
    }
  }
}