pragma Singleton
import QtQuick
import QtQuick.Controls
import FluentControls 1.0 as FC

QtObject {
    id: root

    // 0 system, 1 light, 2 dark. Accessibility choices are application state,
    // not page-local flags.
    property int mode: 0
    property bool highContrast: false
    property bool reducedMotion: false
    readonly property bool dark: mode === 2 ||
                                 (mode === 0 && Application.styleHints.colorScheme === Qt.Dark)

    onDarkChanged: FC.Fluent.setTheme(dark)
    Component.onCompleted: FC.Fluent.setTheme(dark)

    // FluentControls is the runtime color source. Material 3 and macOS kits
    // inform semantic naming, hierarchy and accessibility only.
    readonly property color primary: FC.Fluent.accent
    readonly property color onPrimary: FC.Fluent.textOnAccent
    readonly property color primaryHover: FC.Fluent.accentHover
    readonly property color primaryPressed: FC.Fluent.accentPressed
    readonly property color primaryDisabled: FC.Fluent.accentDisabled
    readonly property color secondary: FC.Fluent.controlStrongFill
    readonly property color onSecondary: FC.Fluent.textPrimary
    readonly property color background: FC.Fluent.background
    readonly property color onBackground: FC.Fluent.textPrimary
    readonly property color surface: FC.Fluent.cardBackground
    readonly property color surfaceRaised: FC.Fluent.cardBackgroundTertiary
    readonly property color surfaceOverlay: FC.Fluent.popupBackground
    readonly property color onSurface: FC.Fluent.textPrimary
    readonly property color onSurfaceVariant: FC.Fluent.textSecondary
    readonly property color text: onSurface
    readonly property color mutedText: onSurfaceVariant
    readonly property color disabledText: FC.Fluent.textDisabled
    readonly property color border: highContrast ? FC.Fluent.controlBorderStrong : FC.Fluent.cardBorder
    readonly property color borderStrong: FC.Fluent.controlBorderStrong
    readonly property color focus: FC.Fluent.focusBorderOuter
    readonly property color selection: FC.Fluent.accentSelected
    readonly property color control: FC.Fluent.controlFill
    readonly property color controlHover: FC.Fluent.controlFillSecondary
    readonly property color controlPressed: FC.Fluent.controlFillTertiary
    readonly property color controlDisabled: FC.Fluent.controlFillQuaternary
    readonly property color positive: FC.Fluent.success
    readonly property color onPositive: dark ? "#000000" : "#ffffff"
    readonly property color warning: FC.Fluent.caution
    readonly property color onWarning: dark ? "#000000" : "#ffffff"
    readonly property color error: FC.Fluent.critical
    readonly property color onError: "#ffffff"
    readonly property color info: FC.Fluent.informational
    readonly property color shadow: FC.Fluent.shadow
    readonly property color shadowAmbient: FC.Fluent.shadowAmbient
    readonly property color accent: primary
    readonly property color accentText: onPrimary

    // 4-point rhythm shared with the supplied Material reference, adapted to
    // Fluent desktop density.
    readonly property int space2: 2
    readonly property int space4: 4
    readonly property int space8: 8
    readonly property int space12: 12
    readonly property int space16: 16
    readonly property int space24: 24
    readonly property int space32: 32
    readonly property int spacingCompact: space8
    readonly property int spacing: space12
    readonly property int spacingWide: space24
    readonly property int pageMarginCompact: space16
    readonly property int pageMargin: space24
    readonly property int pageMarginWide: space32

    readonly property int radiusSmall: FC.Fluent.radius.small
    readonly property int radius: FC.Fluent.radius.medium
    readonly property int radiusLarge: FC.Fluent.radius.large
    readonly property int radiusXLarge: FC.Fluent.radius.xlarge
    readonly property int borderWidth: highContrast ? 2 : 1
    readonly property int elevationNone: 0
    readonly property int elevationCard: 2
    readonly property int elevationPopup: 8
    readonly property int elevationDialog: 16

    // Point-size roles scale with platform DPI. Segoe UI Variable is the
    // Fluent source; the supplied macOS kit establishes the SF Pro fallback.
    readonly property string fontFamily: Qt.platform.os === "osx"
                                         ? "SF Pro Text"
                                         : FC.Fluent.typography.fontFamily
    readonly property real typeDisplay: 36
    readonly property real typeTitleLarge: 24
    readonly property real typeTitle: 18
    readonly property real typeSubtitle: 14
    readonly property real typeBodyLarge: 12
    readonly property real typeBody: 10.5
    readonly property real typeLabel: 10
    readonly property real typeCaption: 9
    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightStrong: Font.DemiBold
    readonly property real lineHeightTight: 1.2
    readonly property real lineHeightBody: 1.4

    readonly property int motionFast: reducedMotion ? 0 : 120
    readonly property int motionNormal: reducedMotion ? 0 : 180
    readonly property int motionSlow: reducedMotion ? 0 : 250
    readonly property int easingStandard: Easing.OutCubic
    readonly property int easingEmphasized: Easing.InOutCubic
}
