# -*- coding: utf-8 -*-
import xml.etree.ElementTree as ET
import os

TRANSLATIONS = {
    "BackgroundRemovalDialog": {
        "Background Removal": {
            "fr_FR": "Suppression d'arrière-plan",
            "en_US": "Background Removal",
            "ja_JA": "背景の削除"
        },
        "Detected Background Color": {
            "fr_FR": "Couleur d'arrière-plan détectée",
            "en_US": "Detected Background Color",
            "ja_JA": "検出された背景色"
        },
        "Removal Parameters": {
            "fr_FR": "Paramètres de détourage",
            "en_US": "Removal Parameters",
            "ja_JA": "切り抜き設定"
        },
        "Color tolerance:": {
            "fr_FR": "Tolérance de couleur :",
            "en_US": "Color tolerance:",
            "ja_JA": "色の許容値："
        },
        "Alpha threshold:": {
            "fr_FR": "Seuil Alpha :",
            "en_US": "Alpha threshold:",
            "ja_JA": "アルファしきい値："
        },
        "Vertical tolerance:": {
            "fr_FR": "Tolérance verticale :",
            "en_US": "Vertical tolerance:",
            "ja_JA": "垂直方向の許容値："
        },
        "Smart Crop": {
            "fr_FR": "Smart Crop",
            "en_US": "Smart Crop",
            "ja_JA": "スマートクロップ"
        },
        "Automatically shrink-wrap bounding boxes around opaque sprite pixels": {
            "fr_FR": "Ajuste automatiquement les rectangles de découpe autour des zones opaques",
            "en_US": "Automatically shrink-wrap bounding boxes around opaque sprite pixels",
            "ja_JA": "不透明なスプライトピクセルの周囲に自動的に境界ボックスを合わせます"
        },
        "Overlap threshold:": {
            "fr_FR": "Seuil chevauchement :",
            "en_US": "Overlap threshold:",
            "ja_JA": "重複しきい値："
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "%1 initial frame(s)": {
            "fr_FR": "%1 frame(s) d'origine",
            "en_US": "%1 initial frame(s)",
            "ja_JA": "%1 元のフレーム"
        }
    },
    "BackgroundRemovalFilter": {
        "Background Removal...": {
            "fr_FR": "Suppression d'arrière-plan...",
            "en_US": "Background Removal...",
            "ja_JA": "背景の削除..."
        },
        "Detects dominant background color and makes pixels transparent with automatic bounding box recalculation.": {
            "fr_FR": "Détecte la couleur dominante du fond et rend les pixels transparents avec recalcul automatique des boîtes englobantes.",
            "en_US": "Detects dominant background color and makes pixels transparent with automatic bounding box recalculation.",
            "ja_JA": "主要な背景色を検出して透明化し、境界ボックスを自動再計算します。"
        }
    },
    "DespillFilter": {
        "Despill & Edge Cleanup...": {
            "fr_FR": "Débavurage & Anti-Halo...",
            "en_US": "Despill & Edge Cleanup...",
            "ja_JA": "エッジクリーンアップ・アンチハロー..."
        },
        "Eliminates 1px colored fringe (green, white, magenta) along sprite borders after background extraction.": {
            "fr_FR": "Élimine le liseré coloré (vert, blanc, magenta) de 1 px persistant sur le pourtour des sprites après détourage.",
            "en_US": "Eliminates 1px colored fringe (green, white, magenta) along sprite borders after background extraction.",
            "ja_JA": "背景切り抜き後にスプライトの輪郭に残る1pxのカラーフリンジ（緑、白、マゼンタ等）を除去します。"
        }
    },
    "DespillFilterDialog": {
        "Despill & Edge Cleanup": {
            "fr_FR": "Débavurage & Anti-Halo",
            "en_US": "Despill & Edge Cleanup",
            "ja_JA": "エッジクリーンアップ・アンチハロー"
        },
        "Fringe color (Halo):": {
            "fr_FR": "Couleur du liseré (Halo) :",
            "en_US": "Fringe color (Halo):",
            "ja_JA": "フリンジカラー（ハロー）："
        },
        "Pick...": {
            "fr_FR": "Choisir...",
            "en_US": "Pick...",
            "ja_JA": "選択..."
        },
        "Select the peripheral fringe color to eliminate": {
            "fr_FR": "Sélectionner la couleur du halo périphérique à éliminer",
            "en_US": "Select the peripheral fringe color to eliminate",
            "ja_JA": "除去する周辺のハロー色を選択します"
        },
        "Action mode:": {
            "fr_FR": "Mode d'action :",
            "en_US": "Action mode:",
            "ja_JA": "処理モード："
        },
        "Soft Color Clamping (Recommended - Preserves fine edges)": {
            "fr_FR": "Color Clamping doux (Recommandé - Conserve les contours)",
            "en_US": "Soft Color Clamping (Recommended - Preserves fine edges)",
            "ja_JA": "ソフトカラークランプ（推奨 - 輪郭を保持）"
        },
        "Strict removal (Alpha = 0)": {
            "fr_FR": "Suppression stricte (Alpha = 0)",
            "en_US": "Strict removal (Alpha = 0)",
            "ja_JA": "厳密な削除（アルファ = 0）"
        },
        "Soft clamping replaces fringe hue with interior neighbor color.\nStrict removal clears the pixel.": {
            "fr_FR": "Le clamping doux remplace la couleur du liseré par la couleur intérieure adjacente.\nLa suppression stricte efface le pixel.",
            "en_US": "Soft clamping replaces fringe hue with interior neighbor color.\nStrict removal clears the pixel.",
            "ja_JA": "ソフトクランプはフリンジ色を隣接する内部色で置換します。\n厳密な削除はピクセルを透明化します。"
        },
        "Detection tolerance (0 - 100):": {
            "fr_FR": "Tolérance de détection (0 - 100) :",
            "en_US": "Detection tolerance (0 - 100):",
            "ja_JA": "検出許容値 (0 - 100)："
        },
        "Apply to selected frames only": {
            "fr_FR": "Appliquer uniquement aux frames sélectionnées",
            "en_US": "Apply to selected frames only",
            "ja_JA": "選択したフレームにのみ適用"
        },
        "No frames selected: applies to entire atlas": {
            "fr_FR": "Aucune frame sélectionnée : s'applique à tout l'atlas",
            "en_US": "No frames selected: applies to entire atlas",
            "ja_JA": "フレームが選択されていません：アトラス全体に適用されます"
        },
        "Select Fringe Color": {
            "fr_FR": "Choisir la couleur du liseré",
            "en_US": "Select Fringe Color",
            "ja_JA": "フリンジ色の選択"
        },
        "%1 fringe pixel(s) processed": {
            "fr_FR": "%1 pixel(s) de liseré traités",
            "en_US": "%1 fringe pixel(s) processed",
            "ja_JA": "%1 個のフリンジピクセルを処理しました"
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "Filter: Despill & Edge Cleanup": {
            "fr_FR": "Filtre : Débavurage & Anti-Halo",
            "en_US": "Filter: Despill & Edge Cleanup",
            "ja_JA": "フィルター：エッジクリーンアップ・アンチハロー"
        }
    },
    "OutlineFilter": {
        "Outline & Silhouette Generator...": {
            "fr_FR": "Générateur de Contours & Silhouettes...",
            "en_US": "Outline & Silhouette Generator...",
            "ja_JA": "輪郭・シルエットジェネレーター..."
        },
        "Adds a distinct outline (1-4 px) around sprites (sticker effect, visibility) or creates solid silhouettes (hit-flash).": {
            "fr_FR": "Ajoute un contour marqué (1-4 px) autour des sprites (effet sticker, lisibilité) ou produit des silhouettes pleines (hit-flash).",
            "en_US": "Adds a distinct outline (1-4 px) around sprites (sticker effect, visibility) or creates solid silhouettes (hit-flash).",
            "ja_JA": "スプライトの周囲に輪郭線（1〜4px）を追加したり、被弾フラッシュ用の塗りつぶしシルエットを生成します。"
        }
    },
    "OutlineFilterDialog": {
        "Outline & Silhouette Generator": {
            "fr_FR": "Générateur de Contours & Silhouettes",
            "en_US": "Outline & Silhouette Generator",
            "ja_JA": "輪郭・シルエットジェネレーター"
        },
        "Outline color:": {
            "fr_FR": "Couleur du contour :",
            "en_US": "Outline color:",
            "ja_JA": "輪郭の色："
        },
        "Pick...": {
            "fr_FR": "Choisir...",
            "en_US": "Pick...",
            "ja_JA": "選択..."
        },
        "Black": {
            "fr_FR": "Noir",
            "en_US": "Black",
            "ja_JA": "黒"
        },
        "White": {
            "fr_FR": "Blanc",
            "en_US": "White",
            "ja_JA": "白"
        },
        "Gold": {
            "fr_FR": "Or",
            "en_US": "Gold",
            "ja_JA": "ゴールド"
        },
        "Stroke thickness (1 to 4 px):": {
            "fr_FR": "Épaisseur du trait (1 à 4 px) :",
            "en_US": "Stroke thickness (1 to 4 px):",
            "ja_JA": "線の太さ (1 〜 4 px)："
        },
        "Connectivity:": {
            "fr_FR": "Connectivité :",
            "en_US": "Connectivity:",
            "ja_JA": "接続性："
        },
        "4-connected (Orthogonal crisp - Retro pixel art)": {
            "fr_FR": "4-connecté (Orthogonal net - Pixel Art rétro)",
            "en_US": "4-connected (Orthogonal crisp - Retro pixel art)",
            "ja_JA": "4近傍（直交シャープ - レトロピクセルアート）"
        },
        "8-connected (Diagonal included - Smooth outline)": {
            "fr_FR": "8-connecté (Avec diagonales - Contour continu)",
            "en_US": "8-connected (Diagonal included - Smooth outline)",
            "ja_JA": "8近傍（対角線含む - 滑らかな輪郭）"
        },
        "Solid silhouette / Hit-flash (Fill sprite interior)": {
            "fr_FR": "Silhouette pleine / Hit-flash (Remplit l'intérieur du sprite)",
            "en_US": "Solid silhouette / Hit-flash (Fill sprite interior)",
            "ja_JA": "塗りつぶしシルエット / 被弾フラッシュ（内部を塗りつぶす）"
        },
        "Apply to selected frames only": {
            "fr_FR": "Appliquer uniquement aux frames sélectionnées",
            "en_US": "Apply to selected frames only",
            "ja_JA": "選択したフレームにのみ適用"
        },
        "No frames selected: applies to entire atlas": {
            "fr_FR": "Aucune frame sélectionnée : s'applique à tout l'atlas",
            "en_US": "No frames selected: applies to entire atlas",
            "ja_JA": "フレームが選択されていません：アトラス全体に適用されます"
        },
        "Select Outline Color": {
            "fr_FR": "Choisir la couleur de contour",
            "en_US": "Select Outline Color",
            "ja_JA": "輪郭色の選択"
        },
        "Outline %1 px applied": {
            "fr_FR": "Contour %1 px appliqué",
            "en_US": "Outline %1 px applied",
            "ja_JA": "輪郭線 %1 px を適用しました"
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "Filter: Outline & Silhouette": {
            "fr_FR": "Filtre : Contour & Silhouette",
            "en_US": "Filter: Outline & Silhouette",
            "ja_JA": "フィルター：輪郭・シルエット"
        }
    },
    "ColorSwapFilter": {
        "Color Swap (Alt-Skins)...": {
            "fr_FR": "Échange de Palette (Alt-Skins)...",
            "en_US": "Color Swap (Alt-Skins)...",
            "ja_JA": "カラー置換（Altスキン）..."
        },
        "Generates character/monster variants (Player 2, elemental skins) by swapping colors while preserving shading.": {
            "fr_FR": "Génère des variantes de personnages ou monstres (Joueur 2, variantes élémentaires) en permutant les couleurs tout en préservant le shading.",
            "en_US": "Generates character/monster variants (Player 2, elemental skins) by swapping colors while preserving shading.",
            "ja_JA": "陰影を保持したまま配色を置換し、キャラクターやモンスターのカラーバリエーション（2Pカラー、属性バリエーション等）を生成します。"
        }
    },
    "ColorSwapFilterDialog": {
        "Color Swap & Alt-Skins": {
            "fr_FR": "Échange de Palette & Variantes",
            "en_US": "Color Swap & Alt-Skins",
            "ja_JA": "カラー置換・パレットバリエーション"
        },
        "Source color to replace:": {
            "fr_FR": "Couleur source à remplacer :",
            "en_US": "Source color to replace:",
            "ja_JA": "置換元の色："
        },
        "Pick...": {
            "fr_FR": "Choisir...",
            "en_US": "Pick...",
            "ja_JA": "選択..."
        },
        "Target new color:": {
            "fr_FR": "Nouvelle couleur cible :",
            "en_US": "Target new color:",
            "ja_JA": "置換先の新しい色："
        },
        "Hue / Color tolerance (0 - 100):": {
            "fr_FR": "Tolérance de teinte / couleur (0 - 100) :",
            "en_US": "Hue / Color tolerance (0 - 100):",
            "ja_JA": "色相・色の許容値 (0 - 100)："
        },
        "Preserve shading (Original shadows, gradients and highlights)": {
            "fr_FR": "Préserver le shading (Ombres, dégradés et reflets originaux)",
            "en_US": "Preserve shading (Original shadows, gradients and highlights)",
            "ja_JA": "シェーディングを保持（元の影・グラデーション・ハイライト）"
        },
        "Swaps the hue while adapting relative lightness to keep pixel art depth and shading.": {
            "fr_FR": "Permute la teinte tout en adaptant la luminosité relative pour conserver les ombres du pixel art.",
            "en_US": "Swaps the hue while adapting relative lightness to keep pixel art depth and shading.",
            "ja_JA": "ピクセルアートの立体感と陰影を維持しながら色相を置換します。"
        },
        "Apply to selected frames only": {
            "fr_FR": "Appliquer uniquement aux frames sélectionnées",
            "en_US": "Apply to selected frames only",
            "ja_JA": "選択したフレームにのみ適用"
        },
        "No frames selected: applies to entire atlas": {
            "fr_FR": "Aucune frame sélectionnée : s'applique à tout l'atlas",
            "en_US": "No frames selected: applies to entire atlas",
            "ja_JA": "フレームが選択されていません：アトラス全体に適用されます"
        },
        "Select source color to replace": {
            "fr_FR": "Sélectionner la couleur source à remplacer",
            "en_US": "Select source color to replace",
            "ja_JA": "置換元の色を選択"
        },
        "Select new target color": {
            "fr_FR": "Sélectionner la nouvelle couleur",
            "en_US": "Select new target color",
            "ja_JA": "置換先の新しい色を選択"
        },
        "%1 pixel(s) modified": {
            "fr_FR": "%1 pixel(s) modifiés",
            "en_US": "%1 pixel(s) modified",
            "ja_JA": "%1 個のピクセルを変更しました"
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "Filter: Color Swap": {
            "fr_FR": "Filtre : Échange de Palette",
            "en_US": "Filter: Color Swap",
            "ja_JA": "フィルター：カラー置換"
        }
    },
    "FilterDialogBase": {
        "Live Preview": {
            "fr_FR": "Aperçu en direct",
            "en_US": "Live Preview",
            "ja_JA": "リアルタイムプレビュー"
        },
        "Update atlas and frames in real-time while adjusting parameters": {
            "fr_FR": "Mettre à jour l'atlas et les frames en temps réel pendant le réglage",
            "en_US": "Update atlas and frames in real-time while adjusting parameters",
            "ja_JA": "調整中にアトラスとフレームをリアルタイムで更新します"
        },
        "Auto-detect Sprite Boxes": {
            "fr_FR": "Détection auto des boîtes",
            "en_US": "Auto-detect Sprite Boxes",
            "ja_JA": "スプライト枠の自動検出"
        },
        "Automatically recalculate sprite bounding boxes after filtering": {
            "fr_FR": "Recalcule automatiquement les boîtes englobantes des sprites après filtrage",
            "en_US": "Automatically recalculate sprite bounding boxes after filtering",
            "ja_JA": "フィルター適用後にスプライトの境界ボックスを自動再計算します"
        },
        "Reset Defaults": {
            "fr_FR": "Valeurs par défaut",
            "en_US": "Reset Defaults",
            "ja_JA": "デフォルトに戻す"
        },
        "Restore recommended default values for this filter": {
            "fr_FR": "Rétablir les valeurs recommandées pour ce filtre",
            "en_US": "Restore recommended default values for this filter",
            "ja_JA": "このフィルターの推奨デフォルト値に戻します"
        },
        "%1 initial frame(s)": {
            "fr_FR": "%1 frame(s) d'origine",
            "en_US": "%1 initial frame(s)",
            "ja_JA": "%1 元のフレーム"
        },
        "OK": {
            "fr_FR": "OK",
            "en_US": "OK",
            "ja_JA": "OK"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        }
    },
    "FilterRegistry": {
        "Cleanup & Extraction": {
            "fr_FR": "Nettoyage & Extraction",
            "en_US": "Cleanup & Extraction",
            "ja_JA": "クリーンアップ・抽出"
        },
        "Effects & Outlines": {
            "fr_FR": "Effets & Bordures",
            "en_US": "Effects & Outlines",
            "ja_JA": "エフェクト・輪郭"
        },
        "Colors & Palettes": {
            "fr_FR": "Couleurs & Palettes",
            "en_US": "Colors & Palettes",
            "ja_JA": "カラー・パレット"
        }
    },
    "GitCommitNodeItem": {
        "KEY_GIT_TIP_MSG": {
            "fr_FR": "Message :",
            "en_US": "Message:",
            "ja_JA": "メッセージ："
        },
        "KEY_GIT_TIP_AUTHOR": {
            "fr_FR": "Auteur :",
            "en_US": "Author:",
            "ja_JA": "作成者："
        },
        "KEY_GIT_TIP_DATE": {
            "fr_FR": "Date :",
            "en_US": "Date:",
            "ja_JA": "日時："
        },
        "KEY_GIT_TIP_RESTORE_HINT": {
            "fr_FR": "Double-clic pour restaurer cette révision",
            "en_US": "Double-click to restore this revision",
            "ja_JA": "ダブルクリックでこのリビジョンを復元"
        }
    },
    "MainWindow": {
        "KEY_GRP_PIVOT": {
            "fr_FR": "Point d'ancrage (Pivot)",
            "en_US": "Anchor & Pivot",
            "ja_JA": "原点・ピボット"
        },
        "KEY_TOOLTIP_PIVOT_GROUND": {
            "fr_FR": "Définir le pivot au Sol (Bas-Centre)",
            "en_US": "Set pivot to Ground (Bottom-Center)",
            "ja_JA": "ピボットを地面（中央下）に設定"
        },
        "KEY_TOOLTIP_PIVOT_CENTER": {
            "fr_FR": "Définir le pivot au Centre",
            "en_US": "Set pivot to Center",
            "ja_JA": "ピボットを中央に設定"
        },
        "KEY_TOOLTIP_PIVOT_TOPLEFT": {
            "fr_FR": "Définir le pivot en Haut-Gauche (UI)",
            "en_US": "Set pivot to Top-Left (UI)",
            "ja_JA": "ピボットを左上（UI）に設定"
        },
        "KEY_TOOLTIP_SHOW_RETICLE": {
            "fr_FR": "Afficher/masquer la mire de pivot et la ligne de sol",
            "en_US": "Show/hide pivot reticle and ground line",
            "ja_JA": "ピボットレティクルとグラウンドラインを表示/非表示"
        },
        "KEY_PIVOT_TOPLEFT": {
            "fr_FR": "Haut-Gauche",
            "en_US": "Top-Left",
            "ja_JA": "左上"
        },
        "KEY_PIVOT_TOPCENTER": {
            "fr_FR": "Haut-Centre",
            "en_US": "Top-Center",
            "ja_JA": "中央上"
        },
        "KEY_PIVOT_TOPRIGHT": {
            "fr_FR": "Haut-Droite",
            "en_US": "Top-Right",
            "ja_JA": "右上"
        },
        "KEY_PIVOT_CENTERLEFT": {
            "fr_FR": "Centre-Gauche",
            "en_US": "Center-Left",
            "ja_JA": "中央左"
        },
        "KEY_PIVOT_CENTER": {
            "fr_FR": "Centre",
            "en_US": "Center",
            "ja_JA": "中央"
        },
        "KEY_PIVOT_CENTERRIGHT": {
            "fr_FR": "Centre-Droite",
            "en_US": "Center-Right",
            "ja_JA": "中央右"
        },
        "KEY_PIVOT_BOTTOMLEFT": {
            "fr_FR": "Bas-Gauche",
            "en_US": "Bottom-Left",
            "ja_JA": "左下"
        },
        "KEY_PIVOT_BOTTOMCENTER": {
            "fr_FR": "Bas-Centre",
            "en_US": "Bottom-Center",
            "ja_JA": "中央下"
        },
        "KEY_PIVOT_BOTTOMRIGHT": {
            "fr_FR": "Bas-Droite",
            "en_US": "Bottom-Right",
            "ja_JA": "右下"
        },
        "KEY_PIVOT_CUSTOM": {
            "fr_FR": "Personnalisé",
            "en_US": "Custom",
            "ja_JA": "カスタム"
        },
        "KEY_BTN_APPLY_PIVOT_ANIM": {
            "fr_FR": "Appliquer à l'anim",
            "en_US": "Apply to Anim",
            "ja_JA": "アニメに適用"
        },
        "KEY_TOOLTIP_APPLY_PIVOT_ANIM": {
            "fr_FR": "Appliquer ce point de pivot à toutes les frames de l'animation courante",
            "en_US": "Apply this pivot point to all frames in the current animation",
            "ja_JA": "現在のアニメーションのすべてのフレームにこのピボットを適用"
        },
        "KEY_BTN_APPLY_PIVOT_ALL": {
            "fr_FR": "Appliquer à tous",
            "en_US": "Apply to All",
            "ja_JA": "すべてに適用"
        },
        "KEY_TOOLTIP_APPLY_PIVOT_ALL": {
            "fr_FR": "Appliquer ce point de pivot à toutes les frames du projet",
            "en_US": "Apply this pivot point to all frames in the project",
            "ja_JA": "プロジェクト内のすべてのフレームにこのピボットを適用"
        },
        "KEY_CTX_PIVOT_SUBMENU": {
            "fr_FR": "Point d'ancrage (Pivot)",
            "en_US": "Anchor & Pivot",
            "ja_JA": "原点・ピボット"
        },
        "KEY_PIVOT_GROUND_HINT": {
            "fr_FR": "Sol",
            "en_US": "Ground",
            "ja_JA": "地面"
        },
        "KEY_PIVOT_UI_HINT": {
            "fr_FR": "UI",
            "en_US": "UI",
            "ja_JA": "UI"
        },
        "KEY_CTX_MORE_PRESETS": {
            "fr_FR": "Autres presets...",
            "en_US": "More presets...",
            "ja_JA": "その他のプリセット..."
        },
        "⬇️ Sol": {
            "fr_FR": "⬇️ Sol",
            "en_US": "⬇️ Ground",
            "ja_JA": "⬇️ 地面"
        },
        "🎯 Centre": {
            "fr_FR": "🎯 Centre",
            "en_US": "🎯 Center",
            "ja_JA": "🎯 中央"
        },
        "↖️ UI": {
            "fr_FR": "↖️ UI",
            "en_US": "↖️ UI",
            "ja_JA": "↖️ UI"
        },
        "🎯 Mire": {
            "fr_FR": "🎯 Mire",
            "en_US": "🎯 Reticle",
            "ja_JA": "🎯 レティクル"
        },
        "KEY_MENU_FILTERS": {
            "fr_FR": "&Filtres",
            "en_US": "&Filters",
            "ja_JA": "フィルター(&F)"
        },
        "KEY_UNKNOWN_FEATURE": {
            "fr_FR": "Fonctionnalité inconnue",
            "en_US": "Unknown feature",
            "ja_JA": "不明な機能"
        },
        "|◀": {
            "fr_FR": "|◀",
            "en_US": "|◀",
            "ja_JA": "|◀"
        },
        "◀": {
            "fr_FR": "◀",
            "en_US": "◀",
            "ja_JA": "◀"
        },
        "▶|": {
            "fr_FR": "▶|",
            "en_US": "▶|",
            "ja_JA": "▶|"
        },
        "0 frame": {
            "fr_FR": "0 frame",
            "en_US": "0 frames",
            "ja_JA": "0 フレーム"
        },
        "FPS:": {
            "fr_FR": "FPS :",
            "en_US": "FPS:",
            "ja_JA": "FPS："
        },
        " -> Timing: 83.33ms": {
            "fr_FR": " -> Intervalle : 83.33ms",
            "en_US": " -> Timing: 83.33ms",
            "ja_JA": " -> 間隔：83.33ms"
        },
        "+": {
            "fr_FR": "+",
            "en_US": "+",
            "ja_JA": "+"
        },
        "+ Sel": {
            "fr_FR": "+ Sél",
            "en_US": "+ Sel",
            "ja_JA": "+ 選択"
        },
        "📋": {
            "fr_FR": "📋",
            "en_US": "📋",
            "ja_JA": "📋"
        },
        "⇄": {
            "fr_FR": "⇄",
            "en_US": "⇄",
            "ja_JA": "⇄"
        },
        "🗑": {
            "fr_FR": "🗑",
            "en_US": "🗑",
            "ja_JA": "🗑"
        },
        "Atlas Bin-Packing": {
            "fr_FR": "Empaquetage d'Atlas",
            "en_US": "Atlas Bin-Packing",
            "ja_JA": "アトラスビンパッキング"
        },
        "Please open or import a sprite sheet with frames first.": {
            "fr_FR": "Veuillez d'abord ouvrir ou importer une planche de sprites avec des frames.",
            "en_US": "Please open or import a sprite sheet with frames first.",
            "ja_JA": "最初にフレームを含むスプライトシートを開くかインポートしてください。"
        },
        "⛶": {
            "fr_FR": "⛶",
            "en_US": "⛶",
            "ja_JA": "⛶"
        },
        "1:1": {
            "fr_FR": "1:1",
            "en_US": "1:1",
            "ja_JA": "1:1"
        },
        "Preset:": {
            "fr_FR": "Preset :",
            "en_US": "Preset:",
            "ja_JA": "プリセット："
        },
        "X:": {
            "fr_FR": "X :",
            "en_US": "X:",
            "ja_JA": "X："
        },
        "Y:": {
            "fr_FR": "Y :",
            "en_US": "Y:",
            "ja_JA": "Y："
        },
        "KEY_ACTION_POLYGON_MESH": {
            "fr_FR": "Maillage polygonal 2D (Tight Mesh)...",
            "en_US": "2D Tight Mesh (Polygon Packing)...",
            "ja_JA": "2Dポリゴンメッシュ（タイトパッキング）..."
        },
        "KEY_ACTION_TOGGLE_POLYGON_MESH": {
            "fr_FR": "Afficher les maillages polygonaux (Wireframe)",
            "en_US": "Show Polygon Meshes (Wireframe)",
            "ja_JA": "ポリゴンメッシュを表示（ワイヤーフレーム）"
        },
        "KEY_CTX_POLYGON_MESH": {
            "fr_FR": "Maillage polygonal 2D (Tight Mesh)...",
            "en_US": "2D Tight Mesh (Polygon Packing)...",
            "ja_JA": "2Dポリゴンメッシュ（タイトパッキング）..."
        },
        "KEY_ACTION_PIXEL_EDITOR": {
            "fr_FR": "Éditeur de pixels (Sprite)...",
            "en_US": "Pixel Editor (Sprite)...",
            "ja_JA": "ピクセルエディタ（スプライト）..."
        },
        "KEY_CTX_EDIT_PIXELS": {
            "fr_FR": "Éditer les pixels...",
            "en_US": "Edit Pixels...",
            "ja_JA": "ピクセルを編集..."
        }
    },
    "PixelEditorDialog": {
        "Pixel Editor — SpriteStudio": {
            "fr_FR": "Éditeur de pixels — SpriteStudio",
            "en_US": "Pixel Editor — SpriteStudio",
            "ja_JA": "ピクセルエディタ — SpriteStudio"
        },
        "◀ Previous Frame": {
            "fr_FR": "◀ Frame précédente",
            "en_US": "◀ Previous Frame",
            "ja_JA": "◀ 前のフレーム"
        },
        "Navigate to previous frame (Page Up)": {
            "fr_FR": "Naviguer vers la frame précédente (Page Haut)",
            "en_US": "Navigate to previous frame (Page Up)",
            "ja_JA": "前のフレームに移動 (Page Up)"
        },
        "Next Frame ▶": {
            "fr_FR": "Frame suivante ▶",
            "en_US": "Next Frame ▶",
            "ja_JA": "次のフレーム ▶"
        },
        "Navigate to next frame (Page Down)": {
            "fr_FR": "Naviguer vers la frame suivante (Page Bas)",
            "en_US": "Navigate to next frame (Page Down)",
            "ja_JA": "次のフレームに移動 (Page Down)"
        },
        "Frame 1 / 1 (32x32 px)": {
            "fr_FR": "Frame 1 / 1 (32x32 px)",
            "en_US": "Frame 1 / 1 (32x32 px)",
            "ja_JA": "フレーム 1 / 1 (32x32 px)"
        },
        "Frame %1 / %2  (%3x%4 px)": {
            "fr_FR": "Frame %1 / %2  (%3x%4 px)",
            "en_US": "Frame %1 / %2  (%3x%4 px)",
            "ja_JA": "フレーム %1 / %2  (%3x%4 px)"
        },
        "Pencil (1px continuous Bresenham) [P]": {
            "fr_FR": "Crayon (Bresenham 1px continu) [P]",
            "en_US": "Pencil (1px continuous Bresenham) [P]",
            "ja_JA": "鉛筆（1px連続ブレゼンハム）[P]"
        },
        "Eraser (1px clear to alpha 0) [E]": {
            "fr_FR": "Gomme (1px effacement vers alpha 0) [E]",
            "en_US": "Eraser (1px clear to alpha 0) [E]",
            "ja_JA": "消しゴム（1px透明化・アルファ0）[E]"
        },
        "Eyedropper / Pipette (Alt+Click or [I])": {
            "fr_FR": "Pipette (Alt+Clic ou [I])",
            "en_US": "Eyedropper / Pipette (Alt+Click or [I])",
            "ja_JA": "スポイト（Alt+クリック または [I]）"
        },
        "Bucket Fill (Flood Fill 4-way) [G]": {
            "fr_FR": "Remplissage (Flot 4-connecté) [G]",
            "en_US": "Bucket Fill (Flood Fill 4-way) [G]",
            "ja_JA": "塗りつぶしバケツ（4方向フローフィル）[G]"
        },
        "Rectangular Marquee Selection [M]": {
            "fr_FR": "Sélection rectangulaire [M]",
            "en_US": "Rectangular Marquee Selection [M]",
            "ja_JA": "長方形選択 [M]"
        },
        "Magic Wand (Color Selection) [W]": {
            "fr_FR": "Baguette magique (Sélection par couleur) [W]",
            "en_US": "Magic Wand (Color Selection) [W]",
            "ja_JA": "自動選択ツール（カラー選択）[W]"
        },
        "Flip Horizontal": {
            "fr_FR": "Miroir horizontal",
            "en_US": "Flip Horizontal",
            "ja_JA": "左右反転"
        },
        "Flip Vertical": {
            "fr_FR": "Miroir vertical",
            "en_US": "Flip Vertical",
            "ja_JA": "上下反転"
        },
        "Rotate 90° Clockwise": {
            "fr_FR": "Rotation 90° sens horaire",
            "en_US": "Rotate 90° Clockwise",
            "ja_JA": "時計回りに90度回転"
        },
        "Toggle Pixel Grid": {
            "fr_FR": "Afficher/Masquer la grille de pixels",
            "en_US": "Toggle Pixel Grid",
            "ja_JA": "ピクセルグリッドの切り替え"
        },
        "Zoom In": {
            "fr_FR": "Zoom avant",
            "en_US": "Zoom In",
            "ja_JA": "ズームイン"
        },
        "Zoom Out": {
            "fr_FR": "Zoom arrière",
            "en_US": "Zoom Out",
            "ja_JA": "ズームアウト"
        },
        "Fit to View": {
            "fr_FR": "Ajuster à la vue",
            "en_US": "Fit to View",
            "ja_JA": "ビューに合わせる"
        },
        "Undo (Ctrl+Z)": {
            "fr_FR": "Annuler (Ctrl+Z)",
            "en_US": "Undo (Ctrl+Z)",
            "ja_JA": "元に戻す (Ctrl+Z)"
        },
        "Redo (Ctrl+Y)": {
            "fr_FR": "Rétablir (Ctrl+Y)",
            "en_US": "Redo (Ctrl+Y)",
            "ja_JA": "やり直す (Ctrl+Y)"
        },
        "Active Colors": {
            "fr_FR": "Couleurs actives",
            "en_US": "Active Colors",
            "ja_JA": "アクティブカラー"
        },
        "Primary Color (Left Click to change)": {
            "fr_FR": "Couleur principale (Clic gauche pour changer)",
            "en_US": "Primary Color (Left Click to change)",
            "ja_JA": "前景色（左クリックで変更）"
        },
        "Secondary Color (Left Click to change)": {
            "fr_FR": "Couleur secondaire (Clic gauche pour changer)",
            "en_US": "Secondary Color (Left Click to change)",
            "ja_JA": "背景色（左クリックで変更）"
        },
        "Swap Colors (X)": {
            "fr_FR": "Intervertir les couleurs (X)",
            "en_US": "Swap Colors (X)",
            "ja_JA": "カラー入れ替え (X)"
        },
        "Palette:": {
            "fr_FR": "Palette :",
            "en_US": "Palette:",
            "ja_JA": "パレット："
        },
        "Sprite Colors (Auto)": {
            "fr_FR": "Couleurs du sprite (Auto)",
            "en_US": "Sprite Colors (Auto)",
            "ja_JA": "スプライトの色（自動）"
        },
        "NES / Famicom (54)": {
            "fr_FR": "NES / Famicom (54)",
            "en_US": "NES / Famicom (54)",
            "ja_JA": "ファミコン / NES (54)"
        },
        "SNES / Super Famicom (32)": {
            "fr_FR": "SNES / Super Famicom (32)",
            "en_US": "SNES / Super Famicom (32)",
            "ja_JA": "スーパーファミコン / SNES (32)"
        },
        "Amiga OCS (32)": {
            "fr_FR": "Amiga OCS (32)",
            "en_US": "Amiga OCS (32)",
            "ja_JA": "Amiga OCS (32)"
        },
        "NEC PC-Engine (32)": {
            "fr_FR": "PC-Engine / Turbografx (32)",
            "en_US": "NEC PC-Engine (32)",
            "ja_JA": "PCエンジン (32)"
        },
        "Game Boy DMG (4)": {
            "fr_FR": "Game Boy DMG (4)",
            "en_US": "Game Boy DMG (4)",
            "ja_JA": "ゲームボーイ DMG (4)"
        },
        "PICO-8 (16)": {
            "fr_FR": "PICO-8 (16)",
            "en_US": "PICO-8 (16)",
            "ja_JA": "PICO-8 (16)"
        },
        "Commodore 64 (16)": {
            "fr_FR": "Commodore 64 (16)",
            "en_US": "Commodore 64 (16)",
            "ja_JA": "コモドール64 (16)"
        },
        "1:1 Scale Preview": {
            "fr_FR": "Aperçu échelle 1:1",
            "en_US": "1:1 Scale Preview",
            "ja_JA": "1:1 プレビュー"
        },
        "X: -- , Y: --": {
            "fr_FR": "X : -- , Y : --",
            "en_US": "X: -- , Y: --",
            "ja_JA": "X: -- , Y: --"
        },
        "X: %1 , Y: %2": {
            "fr_FR": "X : %1 , Y : %2",
            "en_US": "X: %1 , Y: %2",
            "ja_JA": "X: %1 , Y: %2"
        },
        "Transparent [alpha: 0]": {
            "fr_FR": "Transparent [alpha : 0]",
            "en_US": "Transparent [alpha: 0]",
            "ja_JA": "透明 [アルファ: 0]"
        },
        "Zoom: 1600%": {
            "fr_FR": "Zoom : 1600%",
            "en_US": "Zoom: 1600%",
            "ja_JA": "ズーム: 1600%"
        },
        "Zoom: %1%": {
            "fr_FR": "Zoom : %1%",
            "en_US": "Zoom: %1%",
            "ja_JA": "ズーム: %1%"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        },
        "Apply": {
            "fr_FR": "Appliquer",
            "en_US": "Apply",
            "ja_JA": "適用"
        },
        "OK": {
            "fr_FR": "OK",
            "en_US": "OK",
            "ja_JA": "OK"
        },
        "Select Primary Color": {
            "fr_FR": "Sélectionner la couleur principale",
            "en_US": "Select Primary Color",
            "ja_JA": "前景色を選択"
        },
        "Select Secondary Color": {
            "fr_FR": "Sélectionner la couleur secondaire",
            "en_US": "Select Secondary Color",
            "ja_JA": "背景色を選択"
        }
    },
    "PixelCanvas": {
        "Clear Pixels": {
            "fr_FR": "Effacer les pixels",
            "en_US": "Clear Pixels",
            "ja_JA": "ピクセルを消去"
        },
        "Paste": {
            "fr_FR": "Coller",
            "en_US": "Paste",
            "ja_JA": "貼り付け"
        },
        "Flip Horizontal": {
            "fr_FR": "Miroir horizontal",
            "en_US": "Flip Horizontal",
            "ja_JA": "左右反転"
        },
        "Flip Vertical": {
            "fr_FR": "Miroir vertical",
            "en_US": "Flip Vertical",
            "ja_JA": "上下反転"
        },
        "Rotate 90°": {
            "fr_FR": "Rotation 90°",
            "en_US": "Rotate 90°",
            "ja_JA": "90度回転"
        },
        "Flood Fill": {
            "fr_FR": "Remplissage",
            "en_US": "Flood Fill",
            "ja_JA": "塗りつぶし"
        },
        "Eraser": {
            "fr_FR": "Gomme",
            "en_US": "Eraser",
            "ja_JA": "消しゴム"
        },
        "Pencil": {
            "fr_FR": "Crayon",
            "en_US": "Pencil",
            "ja_JA": "鉛筆"
        }
    },
    "PolygonMeshDialog": {
        "Tight Mesh & 2D Polygon Packing": {
            "fr_FR": "Maillage polygonal 2D & Découpage serré",
            "en_US": "Tight Mesh & 2D Polygon Packing",
            "ja_JA": "2Dポリゴンメッシュ＆タイトパッキング"
        },
        "Live Preview & Wireframe": {
            "fr_FR": "Aperçu en direct & Wireframe",
            "en_US": "Live Preview & Wireframe",
            "ja_JA": "ライブプレビュー＆ワイヤーフレーム"
        },
        "Polygon & Mesh Simplification": {
            "fr_FR": "Simplification du polygone & maillage",
            "en_US": "Polygon & Mesh Simplification",
            "ja_JA": "ポリゴンとメッシュの簡素化"
        },
        "Approximation Tolerance (ε):": {
            "fr_FR": "Tolérance d'approximation (ε) :",
            "en_US": "Approximation Tolerance (ε):",
            "ja_JA": "近似許容値 (ε)："
        },
        "Alpha Threshold:": {
            "fr_FR": "Seuil Alpha :",
            "en_US": "Alpha Threshold:",
            "ja_JA": "アルファしきい値："
        },
        "Outward Padding:": {
            "fr_FR": "Marge sortante (Padding) :",
            "en_US": "Outward Padding:",
            "ja_JA": "外側パディング："
        },
        "Max Vertices:": {
            "fr_FR": "Sommets maximum :",
            "en_US": "Max Vertices:",
            "ja_JA": "最大頂点数："
        },
        "Overdraw & Performance Dashboard": {
            "fr_FR": "Performances GPU & Overdraw",
            "en_US": "Overdraw & Performance Dashboard",
            "ja_JA": "オーバードローとパフォーマンス"
        },
        "Vertices: --": {
            "fr_FR": "Sommets : --",
            "en_US": "Vertices: --",
            "ja_JA": "頂点数：--"
        },
        "Triangles: --": {
            "fr_FR": "Triangles : --",
            "en_US": "Triangles: --",
            "ja_JA": "三角形数：--"
        },
        "Polygon Area: --": {
            "fr_FR": "Surface du polygone : --",
            "en_US": "Polygon Area: --",
            "ja_JA": "ポリゴン面積：--"
        },
        "GPU Overdraw Saved: --": {
            "fr_FR": "Overdraw GPU économisé : --",
            "en_US": "GPU Overdraw Saved: --",
            "ja_JA": "GPUオーバードロー削減：--"
        },
        "Vertices: %1": {
            "fr_FR": "Sommets : %1",
            "en_US": "Vertices: %1",
            "ja_JA": "頂点数：%1"
        },
        "Triangles: %1": {
            "fr_FR": "Triangles : %1",
            "en_US": "Triangles: %1",
            "ja_JA": "三角形数：%1"
        },
        "Polygon Area: %1 px² (vs %2 px² box)": {
            "fr_FR": "Surface : %1 px² (vs %2 px² boîte)",
            "en_US": "Polygon Area: %1 px² (vs %2 px² box)",
            "ja_JA": "ポリゴン面積：%1 px²（ボックス：%2 px²）"
        },
        "GPU Overdraw Eliminated: %1%": {
            "fr_FR": "Overdraw GPU éliminé : %1%",
            "en_US": "GPU Overdraw Eliminated: %1%",
            "ja_JA": "GPUオーバードロー削減：%1%"
        },
        "Apply to Selection": {
            "fr_FR": "Appliquer à la sélection",
            "en_US": "Apply to Selection",
            "ja_JA": "選択範囲に適用"
        },
        "Apply to All Frames": {
            "fr_FR": "Appliquer à toutes les frames",
            "en_US": "Apply to All Frames",
            "ja_JA": "全フレームに適用"
        },
        "Remove Mesh (Reset to Rect)": {
            "fr_FR": "Supprimer le maillage (Rétablir rectangle)",
            "en_US": "Remove Mesh (Reset to Rect)",
            "ja_JA": "メッシュを削除（矩形にリセット）"
        },
        "Close": {
            "fr_FR": "Fermer",
            "en_US": "Close",
            "ja_JA": "閉じる"
        },
        "Target Frame %1: Mesh already applied (%2 vertices, %3 tris)": {
            "fr_FR": "Frame cible %1 : Maillage déjà appliqué (%2 sommets, %3 triangles)",
            "en_US": "Target Frame %1: Mesh already applied (%2 vertices, %3 tris)",
            "ja_JA": "対象フレーム %1：メッシュ適用済み（頂点数 %2、三角形数 %3）"
        },
        "Target Frame %1: Rectangle mode (no mesh applied)": {
            "fr_FR": "Frame cible %1 : Mode rectangle (aucun maillage)",
            "en_US": "Target Frame %1: Rectangle mode (no mesh applied)",
            "ja_JA": "対象フレーム %1：矩形モード（メッシュ未適用）"
        },
        "✓ Mesh applied to %1 frame(s)!": {
            "fr_FR": "✓ Maillage appliqué à %1 frame(s) !",
            "en_US": "✓ Mesh applied to %1 frame(s)!",
            "ja_JA": "✓ %1 フレームにメッシュを適用しました！"
        },
        "✓ Mesh applied to all %1 frames!": {
            "fr_FR": "✓ Maillage appliqué aux %1 frames !",
            "en_US": "✓ Mesh applied to all %1 frames!",
            "ja_JA": "✓ 全 %1 フレームにメッシュを適用しました！"
        },
        "✓ Tight mesh removed. Reverted to rectangle.": {
            "fr_FR": "✓ Maillage supprimé. Rétabli en rectangle.",
            "en_US": "✓ Tight mesh removed. Reverted to rectangle.",
            "ja_JA": "✓ メッシュを削除しました。矩形にリセットされました。"
        }
    },
    "ProjectController": {
        "Import %1": {
            "fr_FR": "Importation %1",
            "en_US": "Import %1",
            "ja_JA": "%1 のインポート"
        },
        "Action executed": {
            "fr_FR": "Action exécutée",
            "en_US": "Action executed",
            "ja_JA": "アクションを実行しました"
        },
        "Project modified": {
            "fr_FR": "Projet modifié",
            "en_US": "Project modified",
            "ja_JA": "プロジェクトが変更されました"
        },
        "Undo: %1": {
            "fr_FR": "Annuler : %1",
            "en_US": "Undo: %1",
            "ja_JA": "元に戻す：%1"
        },
        "Action": {
            "fr_FR": "Action",
            "en_US": "Action",
            "ja_JA": "アクション"
        },
        "No active session workspace.": {
            "fr_FR": "Aucun espace de session actif.",
            "en_US": "No active session workspace.",
            "ja_JA": "アクティブなセッションワークスペースがありません。"
        },
        "Checked out revision %1.": {
            "fr_FR": "Restauration de la révision %1 effectuée.",
            "en_US": "Checked out revision %1.",
            "ja_JA": "リビジョン %1 にチェックアウトしました。"
        }
    },
    "QObject": {
        "KEY_CMD_CHANGE_PIVOT": {
            "fr_FR": "Modifier le point d'ancrage",
            "en_US": "Change Anchor Pivot",
            "ja_JA": "ピボット位置を変更"
        },
        "Rename Animation '%1' to '%2'": {
            "fr_FR": "Renommer l'animation '%1' en '%2'",
            "en_US": "Rename Animation '%1' to '%2'",
            "ja_JA": "アニメーション '%1' を '%2' に名前変更"
        },
        "Duplicate Animation '%1' as '%2'": {
            "fr_FR": "Dupliquer l'animation '%1' sous '%2'",
            "en_US": "Duplicate Animation '%1' as '%2'",
            "ja_JA": "アニメーション '%1' を '%2' として複製"
        },
        "Reorder Frames in Animation '%1'": {
            "fr_FR": "Réordonner les frames de l'animation '%1'",
            "en_US": "Reorder Frames in Animation '%1'",
            "ja_JA": "アニメーション '%1' のフレーム順序を変更"
        },
        "Change Properties for Animation '%1'": {
            "fr_FR": "Modifier les propriétés de l'animation '%1'",
            "en_US": "Change Properties for Animation '%1'",
            "ja_JA": "アニメーション '%1' のプロパティを変更"
        },
        "Remove Background": {
            "fr_FR": "Supprimer l'arrière-plan",
            "en_US": "Remove Background",
            "ja_JA": "背景の削除"
        },
        "Cleanup": {
            "fr_FR": "Nettoyage",
            "en_US": "Cleanup",
            "ja_JA": "クリーンアップ"
        },
        "Colors": {
            "fr_FR": "Couleurs",
            "en_US": "Colors",
            "ja_JA": "カラー"
        },
        "Effects": {
            "fr_FR": "Effets",
            "en_US": "Effects",
            "ja_JA": "エフェクト"
        },
        "Geometry": {
            "fr_FR": "Géométrie",
            "en_US": "Geometry",
            "ja_JA": "ジオメトリ"
        },
        "No Atlas Loaded": {
            "fr_FR": "Aucun atlas chargé",
            "en_US": "No Atlas Loaded",
            "ja_JA": "アトラスが読み込まれていません"
        },
        "Please open or import a sprite sheet first before applying a filter.": {
            "fr_FR": "Veuillez d'abord ouvrir ou importer une planche de sprites avant d'appliquer un filtre.",
            "en_US": "Please open or import a sprite sheet first before applying a filter.",
            "ja_JA": "フィルターを適用する前に、まずスプライトシートを開くかインポートしてください。"
        },
        "Edit Frame %1 Pixels": {
            "fr_FR": "Modifier les pixels de la frame %1",
            "en_US": "Edit Frame %1 Pixels",
            "ja_JA": "フレーム %1 のピクセルを編集"
        },
        "Edit Pixels (%1 Frames)": {
            "fr_FR": "Modifier les pixels (%1 frames)",
            "en_US": "Edit Pixels (%1 Frames)",
            "ja_JA": "ピクセルを編集（%1 フレーム）"
        },
        "Set Polygon Mesh": {
            "fr_FR": "Définir le maillage polygonal",
            "en_US": "Set Polygon Mesh",
            "ja_JA": "ポリゴンメッシュを設定"
        }
    },
    "SessionManager": {
        "Error occurred while adding files to ZIP archive: %1": {
            "fr_FR": "Une erreur s'est produite lors de l'ajout des fichiers à l'archive ZIP : %1",
            "en_US": "Error occurred while adding files to ZIP archive: %1",
            "ja_JA": "ZIPアーカイブへのファイル追加中にエラーが発生しました：%1"
        },
        "No active session workspace.": {
            "fr_FR": "Aucun espace de session actif.",
            "en_US": "No active session workspace.",
            "ja_JA": "アクティブなセッションワークスペースがありません。"
        },
        "Failed to open repository.": {
            "fr_FR": "Échec de l'ouverture du dépôt.",
            "en_US": "Failed to open repository.",
            "ja_JA": "リポジトリを開けませんでした。"
        },
        "Invalid commit hash: %1": {
            "fr_FR": "Hash de commit invalide : %1",
            "en_US": "Invalid commit hash: %1",
            "ja_JA": "無効なコミットハッシュ：%1"
        },
        "Commit not found: %1": {
            "fr_FR": "Commit introuvable : %1",
            "en_US": "Commit not found: %1",
            "ja_JA": "コミットが見つかりません：%1"
        },
        "Failed to set detached HEAD to %1": {
            "fr_FR": "Échec du positionnement de HEAD détaché sur %1",
            "en_US": "Failed to set detached HEAD to %1",
            "ja_JA": "デタッチされたHEADを %1 に設定できませんでした"
        },
        "Checkout tree failed with error code: %1": {
            "fr_FR": "L'extraction de l'arbre a échoué avec le code d'erreur : %1",
            "en_US": "Checkout tree failed with error code: %1",
            "ja_JA": "チェックアウトツリーがエラーコード %1 で失敗しました"
        },
        "Git integration is not compiled in.": {
            "fr_FR": "L'intégration Git n'est pas compilée.",
            "en_US": "Git integration is not compiled in.",
            "ja_JA": "Git連携機能はコンパイルされていません。"
        }
    },
    "TimelineFilmstripWidget": {
        "#%1 (F%2)": {
            "fr_FR": "#%1 (F%2)",
            "en_US": "#%1 (F%2)",
            "ja_JA": "#%1 (F%2)"
        }
    },
    "ColorAdjustFilter": {
        "Color Adjustment (HSV & Contrast)...": {
            "fr_FR": "Ajustement des Couleurs (HSV & Contraste)...",
            "en_US": "Color Adjustment (HSV & Contrast)...",
            "ja_JA": "カラー調整 (HSV・コントラスト)..."
        },
        "Adjusts hue rotation, saturation, brightness, and contrast globally or on selected frames.": {
            "fr_FR": "Ajuste la teinte, la saturation, la luminosité et le contraste globalement ou sur les frames sélectionnées.",
            "en_US": "Adjusts hue rotation, saturation, brightness, and contrast globally or on selected frames.",
            "ja_JA": "色相の回転、彩度、明度、コントラストを全体または選択したフレームに対して調整します。"
        }
    },
    "ColorAdjustFilterDialog": {
        "Color Adjustment (HSV & Contrast)": {
            "fr_FR": "Ajustement des Couleurs (HSV & Contraste)",
            "en_US": "Color Adjustment (HSV & Contrast)",
            "ja_JA": "カラー調整 (HSV・コントラスト)"
        },
        "Adjustment Parameters": {
            "fr_FR": "Paramètres d'ajustement",
            "en_US": "Adjustment Parameters",
            "ja_JA": "調整パラメーター"
        },
        "Hue Shift:": {
            "fr_FR": "Décalage de teinte :",
            "en_US": "Hue Shift:",
            "ja_JA": "色相シフト："
        },
        "Saturation:": {
            "fr_FR": "Saturation :",
            "en_US": "Saturation:",
            "ja_JA": "彩度："
        },
        "Brightness:": {
            "fr_FR": "Luminosité :",
            "en_US": "Brightness:",
            "ja_JA": "明度："
        },
        "Contrast:": {
            "fr_FR": "Contraste :",
            "en_US": "Contrast:",
            "ja_JA": "コントラスト："
        },
        "Target Scope": {
            "fr_FR": "Portée de l'effet",
            "en_US": "Target Scope",
            "ja_JA": "適用範囲"
        },
        "Apply to selected frames only": {
            "fr_FR": "Appliquer uniquement aux frames sélectionnées",
            "en_US": "Apply to selected frames only",
            "ja_JA": "選択したフレームにのみ適用"
        },
        "No frames selected: applies to entire atlas": {
            "fr_FR": "Aucune frame sélectionnée : s'applique à tout l'atlas",
            "en_US": "No frames selected: applies to entire atlas",
            "ja_JA": "フレームが選択されていません：アトラス全体に適用されます"
        },
        "Adjusted (H:%1° S:%2% V:%3% C:%4%)": {
            "fr_FR": "Ajusté (H:%1° S:%2% V:%3% C:%4%)",
            "en_US": "Adjusted (H:%1° S:%2% V:%3% C:%4%)",
            "ja_JA": "調整完了 (H:%1° S:%2% V:%3% C:%4%)"
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "Filter: Color Adjustment": {
            "fr_FR": "Filtre : Ajustement des Couleurs",
            "en_US": "Filter: Color Adjustment",
            "ja_JA": "フィルター：カラー調整"
        }
    },
    "FilterRegistry": {
        "Cleanup & Extraction": {
            "fr_FR": "Nettoyage & Extraction",
            "en_US": "Cleanup & Extraction",
            "ja_JA": "クリーンアップと抽出"
        },
        "Effects & Outlines": {
            "fr_FR": "Effets & Bordures",
            "en_US": "Effects & Outlines",
            "ja_JA": "エフェクトと輪郭"
        },
        "Colors & Palettes": {
            "fr_FR": "Couleurs & Palettes",
            "en_US": "Colors & Palettes",
            "ja_JA": "カラーとパレット"
        },
        "Geometry & Transform": {
            "fr_FR": "Géométrie & Transformations",
            "en_US": "Geometry & Transform",
            "ja_JA": "ジオメトリと変形"
        }
    },
    "PixelRescaleFilter": {
        "Pixel Art Rescale...": {
            "fr_FR": "Redimensionnement Pixel Art...",
            "en_US": "Pixel Art Rescale...",
            "ja_JA": "ピクセルアートリサイズ..."
        },
        "Rescales the atlas cleanly using Nearest-Neighbor (pixel-perfect) or Scale2x (smooth contours).": {
            "fr_FR": "Redimensionne proprement l'atlas via Plus Proche Voisin (pixel-perfect) ou Scale2x (contours lissés).",
            "en_US": "Rescales the atlas cleanly using Nearest-Neighbor (pixel-perfect) or Scale2x (smooth contours).",
            "ja_JA": "最近傍補間（ピクセルパーフェクト）またはScale2x（輪郭補間）を使用してアトラスを綺麗に拡大・縮小します。"
        }
    },
    "PixelRescaleFilterDialog": {
        "Pixel Art Rescale": {
            "fr_FR": "Redimensionnement Pixel Art",
            "en_US": "Pixel Art Rescale",
            "ja_JA": "ピクセルアートリサイズ"
        },
        "Rescale Parameters": {
            "fr_FR": "Paramètres de redimensionnement",
            "en_US": "Rescale Parameters",
            "ja_JA": "リサイズ設定"
        },
        "Scale Factor:": {
            "fr_FR": "Facteur d'échelle :",
            "en_US": "Scale Factor:",
            "ja_JA": "スケール倍率："
        },
        "0.5x (Downscale 50%)": {
            "fr_FR": "0.5x (Réduction 50%)",
            "en_US": "0.5x (Downscale 50%)",
            "ja_JA": "0.5倍 (50% 縮小)"
        },
        "2x (Double 200%)": {
            "fr_FR": "2x (Doubler 200%)",
            "en_US": "2x (Double 200%)",
            "ja_JA": "2倍 (200% 拡大)"
        },
        "3x (Triple 300%)": {
            "fr_FR": "3x (Tripler 300%)",
            "en_US": "3x (Triple 300%)",
            "ja_JA": "3倍 (300% 拡大)"
        },
        "4x (Quadruple 400%)": {
            "fr_FR": "4x (Quadrupler 400%)",
            "en_US": "4x (Quadruple 400%)",
            "ja_JA": "4倍 (400% 拡大)"
        },
        "Resampling Engine:": {
            "fr_FR": "Moteur de rééchantillonnage :",
            "en_US": "Resampling Engine:",
            "ja_JA": "リサンプリングエンジン："
        },
        "Nearest-Neighbor (Sharp / Pixel-Perfect)": {
            "fr_FR": "Plus Proche Voisin (Net / Pixel-Perfect)",
            "en_US": "Nearest-Neighbor (Sharp / Pixel-Perfect)",
            "ja_JA": "最近傍補間 (シャープ / ピクセルパーフェクト)"
        },
        "Scale2x / AdvMAME2x (Smooth Contours)": {
            "fr_FR": "Scale2x / AdvMAME2x (Contours lissés)",
            "en_US": "Scale2x / AdvMAME2x (Smooth Contours)",
            "ja_JA": "Scale2x / AdvMAME2x (滑らかな輪郭)"
        },
        "Atlas Dimensions: %1x%2 -> %3x%4 px": {
            "fr_FR": "Dimensions de l'atlas : %1x%2 -> %3x%4 px",
            "en_US": "Atlas Dimensions: %1x%2 -> %3x%4 px",
            "ja_JA": "アトラス解像度：%1x%2 -> %3x%4 px"
        },
        "Rescaled %1x (%2)": {
            "fr_FR": "Redimensionné %1x (%2)",
            "en_US": "Rescaled %1x (%2)",
            "ja_JA": "リサイズ完了 %1倍 (%2)"
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "Filter: Pixel Art Rescale": {
            "fr_FR": "Filtre : Redimensionnement Pixel Art",
            "en_US": "Filter: Pixel Art Rescale",
            "ja_JA": "フィルター：ピクセルアートリサイズ"
        }
    },
    "RetroPaletteFilter": {
        "Retro Palette & Dithering...": {
            "fr_FR": "Palette Rétro & Tramage (Dithering)...",
            "en_US": "Retro Palette & Dithering...",
            "ja_JA": "レトロパレット＆ディザリング..."
        },
        "Quantizes colors to authentic retro hardware palettes with optional ordered Bayer dithering.": {
            "fr_FR": "Quantifie les couleurs selon d'authentiques palettes rétro avec tramage ordonné de Bayer optionnel.",
            "en_US": "Quantizes colors to authentic retro hardware palettes with optional ordered Bayer dithering.",
            "ja_JA": "本格的なレトロハードウェアパレットに色を減色し、Bayerディザリング（規則的階調表現）を適用します。"
        }
    },
    "RetroPaletteFilterDialog": {
        "Retro Palette & Dithering": {
            "fr_FR": "Palette Rétro & Tramage (Dithering)",
            "en_US": "Retro Palette & Dithering",
            "ja_JA": "レトロパレット＆ディザリング"
        },
        "Retro Hardware Palette": {
            "fr_FR": "Palette Matérielle Rétro",
            "en_US": "Retro Hardware Palette",
            "ja_JA": "レトロハードウェアパレット"
        },
        "Palette Preset:": {
            "fr_FR": "Préréglage de palette :",
            "en_US": "Palette Preset:",
            "ja_JA": "パレットプリセット："
        },
        "Game Boy DMG (4 Greens)": {
            "fr_FR": "Game Boy DMG (4 Verts)",
            "en_US": "Game Boy DMG (4 Greens)",
            "ja_JA": "ゲームボーイ DMG (緑4階調)"
        },
        "Game Boy Pocket (4 Grays)": {
            "fr_FR": "Game Boy Pocket (4 Gris)",
            "en_US": "Game Boy Pocket (4 Grays)",
            "ja_JA": "ゲームボーイポケット (モノクロ4階調)"
        },
        "PICO-8 (16 Colors)": {
            "fr_FR": "PICO-8 (16 Couleurs)",
            "en_US": "PICO-8 (16 Colors)",
            "ja_JA": "PICO-8 (16色)"
        },
        "NES / Famicom (54 Colors)": {
            "fr_FR": "NES / Famicom (54 Couleurs)",
            "en_US": "NES / Famicom (54 Colors)",
            "ja_JA": "ファミコン / NES (54色)"
        },
        "Commodore 64 (16 Colors)": {
            "fr_FR": "Commodore 64 (16 Couleurs)",
            "en_US": "Commodore 64 (16 Colors)",
            "ja_JA": "コモドール64 (16色)"
        },
        "CGA Mode 1 (Cyan/Magenta/White)": {
            "fr_FR": "CGA Mode 1 (Cyan/Magenta/Blanc)",
            "en_US": "CGA Mode 1 (Cyan/Magenta/White)",
            "ja_JA": "CGA モード1 (シアン/マゼンタ/白)"
        },
        "CGA Mode 2 (Red/Green/Yellow)": {
            "fr_FR": "CGA Mode 2 (Rouge/Vert/Jaune)",
            "en_US": "CGA Mode 2 (Red/Green/Yellow)",
            "ja_JA": "CGA モード2 (赤/緑/黄)"
        },
        "Endesga 32 (32 Pixel Art Colors)": {
            "fr_FR": "Endesga 32 (32 Couleurs Pixel Art)",
            "en_US": "Endesga 32 (32 Pixel Art Colors)",
            "ja_JA": "Endesga 32 (32色 ピクセルアート)"
        },
        "Custom / Imported Palette": {
            "fr_FR": "Palette personnalisée / importée",
            "en_US": "Custom / Imported Palette",
            "ja_JA": "カスタム / インポートパレット"
        },
        "Import...": {
            "fr_FR": "Importer...",
            "en_US": "Import...",
            "ja_JA": "インポート..."
        },
        "Import palette from .hex, .gpl, .pal or .png image": {
            "fr_FR": "Importer une palette depuis un fichier .hex, .gpl, .pal ou une image .png",
            "en_US": "Import palette from .hex, .gpl, .pal or .png image",
            "ja_JA": ".hex, .gpl, .pal ファイルまたは .png 画像からパレットをインポート"
        },
        "Ordered Dithering (Bayer Matrix)": {
            "fr_FR": "Tramage ordonné (Matrice de Bayer)",
            "en_US": "Ordered Dithering (Bayer Matrix)",
            "ja_JA": "組織的ディザリング (Bayerマトリクス)"
        },
        "Dither Pattern:": {
            "fr_FR": "Motif de tramage :",
            "en_US": "Dither Pattern:",
            "ja_JA": "ディザーパターン："
        },
        "None (Exact Nearest Match)": {
            "fr_FR": "Aucun (Couleur la plus proche)",
            "en_US": "None (Exact Nearest Match)",
            "ja_JA": "なし (最も近い色に単純減色)"
        },
        "Bayer 2x2 Matrix": {
            "fr_FR": "Matrice Bayer 2x2",
            "en_US": "Bayer 2x2 Matrix",
            "ja_JA": "Bayer 2x2 マトリクス"
        },
        "Bayer 4x4 Matrix (Classic Retro)": {
            "fr_FR": "Matrice Bayer 4x4 (Rétro classique)",
            "en_US": "Bayer 4x4 Matrix (Classic Retro)",
            "ja_JA": "Bayer 4x4 マトリクス (クラシックレトロ)"
        },
        "Bayer 8x8 Matrix (Smooth Gradients)": {
            "fr_FR": "Matrice Bayer 8x8 (Dégradés subtils)",
            "en_US": "Bayer 8x8 Matrix (Smooth Gradients)",
            "ja_JA": "Bayer 8x8 マトリクス (滑らかな階調)"
        },
        "Dither Strength:": {
            "fr_FR": "Intensité du tramage :",
            "en_US": "Dither Strength:",
            "ja_JA": "ディザー強度："
        },
        "Target Scope": {
            "fr_FR": "Portée de l'effet",
            "en_US": "Target Scope",
            "ja_JA": "適用範囲"
        },
        "Apply to selected frames only": {
            "fr_FR": "Appliquer uniquement aux frames sélectionnées",
            "en_US": "Apply to selected frames only",
            "ja_JA": "選択したフレームにのみ適用"
        },
        "No frames selected: applies to entire atlas": {
            "fr_FR": "Aucune frame sélectionnée : s'applique à tout l'atlas",
            "en_US": "No frames selected: applies to entire atlas",
            "ja_JA": "フレームが選択されていません：アトラス全体に適用されます"
        },
        "Import Color Palette": {
            "fr_FR": "Importer une palette de couleurs",
            "en_US": "Import Color Palette",
            "ja_JA": "カラーパレットのインポート"
        },
        "Palette Files (*.hex *.gpl *.pal *.png *.bmp);;All Files (*)": {
            "fr_FR": "Fichiers de palette (*.hex *.gpl *.pal *.png *.bmp);;Tous les fichiers (*)",
            "en_US": "Palette Files (*.hex *.gpl *.pal *.png *.bmp);;All Files (*)",
            "ja_JA": "パレットファイル (*.hex *.gpl *.pal *.png *.bmp);;すべてのファイル (*)"
        },
        "Import Failed": {
            "fr_FR": "Échec de l'importation",
            "en_US": "Import Failed",
            "ja_JA": "インポート失敗"
        },
        "No valid colors found in file.": {
            "fr_FR": "Aucune couleur valide trouvée dans le fichier.",
            "en_US": "No valid colors found in file.",
            "ja_JA": "ファイル内に有効な色情報が見つかりませんでした。"
        },
        "%1 active color(s)": {
            "fr_FR": "%1 couleur(s) active(s)",
            "en_US": "%1 active color(s)",
            "ja_JA": "%1 色のアクティブカラー"
        },
        "Quantized (%1 colors, %2)": {
            "fr_FR": "Quantifié (%1 couleurs, %2)",
            "en_US": "Quantized (%1 colors, %2)",
            "ja_JA": "減色完了 (%1色, %2)"
        },
        "%1 frame(s) detected": {
            "fr_FR": "%1 frame(s) détectée(s)",
            "en_US": "%1 frame(s) detected",
            "ja_JA": "%1 フレーム検出"
        },
        "Filter: Retro Palette & Dithering": {
            "fr_FR": "Filtre : Palette Rétro & Tramage",
            "en_US": "Filter: Retro Palette & Dithering",
            "ja_JA": "フィルター：レトロパレット＆ディザリング"
        }
    },
    "AtlasPackingDialog": {
        "Atlas Bin-Packing (MaxRects)": {
            "fr_FR": "Empaquetage d'Atlas (MaxRects)",
            "en_US": "Atlas Bin-Packing (MaxRects)",
            "ja_JA": "アトラスビンパッキング (MaxRects)"
        },
        "Packing Algorithm": {
            "fr_FR": "Algorithme d'empaquetage",
            "en_US": "Packing Algorithm",
            "ja_JA": "パッキングアルゴリズム"
        },
        "MaxRects — Best Short Side Fit (Default, Recommended)": {
            "fr_FR": "MaxRects — Best Short Side Fit (Défaut, Recommandé)",
            "en_US": "MaxRects — Best Short Side Fit (Default, Recommended)",
            "ja_JA": "MaxRects — Best Short Side Fit (デフォルト、推奨)"
        },
        "MaxRects — Best Area Fit (Maximum Compaction)": {
            "fr_FR": "MaxRects — Best Area Fit (Compacité maximale)",
            "en_US": "MaxRects — Best Area Fit (Maximum Compaction)",
            "ja_JA": "MaxRects — Best Area Fit (最大圧縮)"
        },
        "MaxRects — Best Long Side Fit": {
            "fr_FR": "MaxRects — Best Long Side Fit",
            "en_US": "MaxRects — Best Long Side Fit",
            "ja_JA": "MaxRects — Best Long Side Fit"
        },
        "MaxRects — Bottom Left Rule": {
            "fr_FR": "MaxRects — Règle Bottom Left",
            "en_US": "MaxRects — Bottom Left Rule",
            "ja_JA": "MaxRects — ボトムレフト則"
        },
        "MaxRects — Contact Point Rule": {
            "fr_FR": "MaxRects — Règle Contact Point",
            "en_US": "MaxRects — Contact Point Rule",
            "ja_JA": "MaxRects — コンタクトポイント則"
        },
        "Power of Two Shelf Packer (2^n Dimensions)": {
            "fr_FR": "Empaqueteur en étagère Puissance de Deux (Dimensions 2^n)",
            "en_US": "Power of Two Shelf Packer (2^n Dimensions)",
            "ja_JA": "2の累乗シェルフパッカー (2^n 寸法)"
        },
        "Basic Row / Shelf Packer": {
            "fr_FR": "Empaqueteur basique en ligne / étagère",
            "en_US": "Basic Row / Shelf Packer",
            "ja_JA": "基本行 / シェルフパッカー"
        },
        "Uniform Grid Packer": {
            "fr_FR": "Empaqueteur en grille uniforme",
            "en_US": "Uniform Grid Packer",
            "ja_JA": "均等グリッドパッカー"
        },
        "Spacing & Texture Bleeding Protection": {
            "fr_FR": "Espacement & Protection Anti-Saignement",
            "en_US": "Spacing & Texture Bleeding Protection",
            "ja_JA": "間隔とテクスチャブリード防止"
        },
        "Inner margin between adjacent sprites": {
            "fr_FR": "Marge interne entre sprites adjacents",
            "en_US": "Inner margin between adjacent sprites",
            "ja_JA": "隣接スプライト間の内部余白"
        },
        "Inner Padding:": {
            "fr_FR": "Marge intérieure (Padding) :",
            "en_US": "Inner Padding:",
            "ja_JA": "内部余白 (パディング)："
        },
        "Outer margin around the edges of the atlas": {
            "fr_FR": "Marge extérieure autour de l'atlas",
            "en_US": "Outer margin around the edges of the atlas",
            "ja_JA": "アトラス外枠の余白"
        },
        "Border Padding:": {
            "fr_FR": "Marge de bordure :",
            "en_US": "Border Padding:",
            "ja_JA": "外枠パディング："
        },
        "Repeats border pixels outward (1-2px) to prevent bilinear interpolation artifacts in game engines": {
            "fr_FR": "Répète les pixels de bordure vers l'extérieur (1-2px) pour éviter les artefacts de filtrage bilinéaire dans les moteurs",
            "en_US": "Repeats border pixels outward (1-2px) to prevent bilinear interpolation artifacts in game engines",
            "ja_JA": "ゲームエンジンでのバイリニア補間アーティファクトを防ぐため、境界ピクセルを外側に複製 (1-2px) します"
        },
        "Extrude (Anti-Bleeding):": {
            "fr_FR": "Extrusion (Anti-Saignement) :",
            "en_US": "Extrude (Anti-Bleeding):",
            "ja_JA": "押し出し (ブリード防止)："
        },
        "GPU Constraints & Optimizations": {
            "fr_FR": "Contraintes & Optimisations GPU",
            "en_US": "GPU Constraints & Optimizations",
            "ja_JA": "GPU制約と最適化"
        },
        "Force Power of Two Dimensions (2^n: 512, 1024, 2048...)": {
            "fr_FR": "Forcer les dimensions en Puissance de Deux (2^n : 512, 1024, 2048...)",
            "en_US": "Force Power of Two Dimensions (2^n: 512, 1024, 2048...)",
            "ja_JA": "2の累乗寸法に強制 (2^n: 512, 1024, 2048...)"
        },
        "Force Square Atlas (Width == Height)": {
            "fr_FR": "Forcer un atlas carré (Largeur == Hauteur)",
            "en_US": "Force Square Atlas (Width == Height)",
            "ja_JA": "正方形アトラスに強制 (幅 == 高さ)"
        },
        "Auto-Aliasing (Merge identical frames without breaking animations)": {
            "fr_FR": "Auto-Aliasing (Fusionner les frames identiques sans casser les animations)",
            "en_US": "Auto-Aliasing (Merge identical frames without breaking animations)",
            "ja_JA": "オートエイリアシング (アニメーションを壊さずに同一フレームを統合)"
        },
        "Trim Transparent Borders before packing": {
            "fr_FR": "Rogner les bordures transparentes avant empaquetage",
            "en_US": "Trim Transparent Borders before packing",
            "ja_JA": "パッキング前に透明な境界線をトリミング"
        },
        "Live Packing Metrics": {
            "fr_FR": "Métriques d'Empaquetage en Direct",
            "en_US": "Live Packing Metrics",
            "ja_JA": "リアルタイムパッキング指標"
        },
        "Atlas Dimensions:": {
            "fr_FR": "Dimensions de l'atlas :",
            "en_US": "Atlas Dimensions:",
            "ja_JA": "アトラス寸法："
        },
        "Packing Efficiency:": {
            "fr_FR": "Efficacité d'empaquetage :",
            "en_US": "Packing Efficiency:",
            "ja_JA": "充填効率："
        },
        "Frames Count:": {
            "fr_FR": "Nombre de frames :",
            "en_US": "Frames Count:",
            "ja_JA": "フレーム数："
        },
        "Packing Failed (Exceeded max dimensions)": {
            "fr_FR": "Échec d'empaquetage (Dépassement des dimensions maximales)",
            "en_US": "Packing Failed (Exceeded max dimensions)",
            "ja_JA": "パッキング失敗 (最大寸法を超過)"
        },
        "Error: Cannot fit sprites in atlas": {
            "fr_FR": "Erreur : Impossible d'insérer tous les sprites dans l'atlas",
            "en_US": "Error: Cannot fit sprites in atlas",
            "ja_JA": "エラー：スプライトをアトラスに収めることができません"
        },
        "%1 x %2 px": {
            "fr_FR": "%1 x %2 px",
            "en_US": "%1 x %2 px",
            "ja_JA": "%1 x %2 px"
        },
        "%1 unique / %2 total (%3 frame(s) saved)": {
            "fr_FR": "%1 uniques / %2 au total (%3 frame(s) économisée(s))",
            "en_US": "%1 unique / %2 total (%3 frame(s) saved)",
            "ja_JA": "%1 固有 / 合計 %2 (%3 フレーム節約)"
        },
        "%1 frame(s)": {
            "fr_FR": "%1 frame(s)",
            "en_US": "%1 frame(s)",
            "ja_JA": "%1 フレーム"
        },
        "Packed in %1x%2 (%3%)": {
            "fr_FR": "Empaqueté en %1x%2 (%3%)",
            "en_US": "Packed in %1x%2 (%3%)",
            "ja_JA": "%1x%2 にパッキング (%3%)"
        },
        "Filter: Atlas Bin-Packing (MaxRects)": {
            "fr_FR": "Filtre : Empaquetage d'Atlas (MaxRects)",
            "en_US": "Filter: Atlas Bin-Packing (MaxRects)",
            "ja_JA": "フィルター：アトラスビンパッキング (MaxRects)"
        }
    },
    "AtlasPackingFilter": {
        "Atlas Bin-Packing (MaxRects)...": {
            "fr_FR": "Empaquetage d'Atlas (MaxRects)...",
            "en_US": "Atlas Bin-Packing (MaxRects)...",
            "ja_JA": "アトラスビンパッキング (MaxRects)..."
        },
        "Repacks sprites into a compact atlas using MaxRects, Power of Two, and animation-safe deduplication.": {
            "fr_FR": "Ré-agence les sprites dans un atlas compact via MaxRects, Puissance de Deux et déduplication sans altérer les animations.",
            "en_US": "Repacks sprites into a compact atlas using MaxRects, Power of Two, and animation-safe deduplication.",
            "ja_JA": "MaxRects、2の累乗、アニメーション安全な重複排除を使用してスプライトをコンパクトなアトラスに再配置します。"
        }
    },
    "ExportDialog": {
        "Export Atlas & Animations": {
            "fr_FR": "Exporter l'Atlas & les Animations",
            "en_US": "Export Atlas & Animations",
            "ja_JA": "アトラスとアニメーションのエクスポート"
        },
        "Destination": {
            "fr_FR": "Destination",
            "en_US": "Destination",
            "ja_JA": "出力先"
        },
        "Select export file path...": {
            "fr_FR": "Sélectionner le chemin d'exportation...",
            "en_US": "Select export file path...",
            "ja_JA": "エクスポート先のファイルパスを選択..."
        },
        "Browse...": {
            "fr_FR": "Parcourir...",
            "en_US": "Browse...",
            "ja_JA": "参照..."
        },
        "Format & Packing Algorithm": {
            "fr_FR": "Format & Algorithme d'Empaquetage",
            "en_US": "Format & Packing Algorithm",
            "ja_JA": "フォーマットとパッキングアルゴリズム"
        },
        "Export Format:": {
            "fr_FR": "Format d'exportation :",
            "en_US": "Export Format:",
            "ja_JA": "エクスポート形式："
        },
        "Godot 4 (*.tres + *.png)": {
            "fr_FR": "Godot 4 (*.tres + *.png)",
            "en_US": "Godot 4 (*.tres + *.png)",
            "ja_JA": "Godot 4 (*.tres + *.png)"
        },
        "TexturePacker JSON (*.json + *.png)": {
            "fr_FR": "JSON TexturePacker (*.json + *.png)",
            "en_US": "TexturePacker JSON (*.json + *.png)",
            "ja_JA": "TexturePacker JSON (*.json + *.png)"
        },
        "Aseprite JSON (*.json + *.png)": {
            "fr_FR": "JSON Aseprite (*.json + *.png)",
            "en_US": "Aseprite JSON (*.json + *.png)",
            "ja_JA": "Aseprite JSON (*.json + *.png)"
        },
        "Packing Algorithm:": {
            "fr_FR": "Algorithme d'empaquetage :",
            "en_US": "Packing Algorithm:",
            "ja_JA": "パッキングアルゴリズム："
        },
        "Keep Current Layout (WYSIWYG — As Displayed)": {
            "fr_FR": "Conserver l'agencement actuel (WYSIWYG — Tel quel)",
            "en_US": "Keep Current Layout (WYSIWYG — As Displayed)",
            "ja_JA": "現在の配置を維持 (WYSIWYG — 表示通り)"
        },
        "MaxRects (Best Short Side Fit — Recommended)": {
            "fr_FR": "MaxRects (Best Short Side Fit — Recommandé)",
            "en_US": "MaxRects (Best Short Side Fit — Recommended)",
            "ja_JA": "MaxRects (Best Short Side Fit — 推奨)"
        },
        "MaxRects (Best Area Fit)": {
            "fr_FR": "MaxRects (Best Area Fit)",
            "en_US": "MaxRects (Best Area Fit)",
            "ja_JA": "MaxRects (Best Area Fit)"
        },
        "MaxRects (Best Long Side Fit)": {
            "fr_FR": "MaxRects (Best Long Side Fit)",
            "en_US": "MaxRects (Best Long Side Fit)",
            "ja_JA": "MaxRects (Best Long Side Fit)"
        },
        "MaxRects (Bottom Left)": {
            "fr_FR": "MaxRects (Bottom Left)",
            "en_US": "MaxRects (Bottom Left)",
            "ja_JA": "MaxRects (ボトムレフト)"
        },
        "Power of Two (Row Shelf)": {
            "fr_FR": "Puissance de Deux (Étagère)",
            "en_US": "Power of Two (Row Shelf)",
            "ja_JA": "2の累乗 (行シェルフ)"
        },
        "Row / Shelf": {
            "fr_FR": "Ligne par ligne / Étagère",
            "en_US": "Row / Shelf",
            "ja_JA": "行 / シェルフ"
        },
        "Uniform Grid": {
            "fr_FR": "Grille uniforme",
            "en_US": "Uniform Grid",
            "ja_JA": "均等グリッド"
        },
        "Layout & Margins": {
            "fr_FR": "Disposition & Marges",
            "en_US": "Layout & Margins",
            "ja_JA": "配置と余白"
        },
        "Inner Padding (px):": {
            "fr_FR": "Espacement interne (px) :",
            "en_US": "Inner Padding (px):",
            "ja_JA": "内部パディング (px)："
        },
        "Border Extrude (px):": {
            "fr_FR": "Extrusion de bordure (px) :",
            "en_US": "Border Extrude (px):",
            "ja_JA": "境界押し出し (px)："
        },
        "Border Padding (px):": {
            "fr_FR": "Marge extérieure (px) :",
            "en_US": "Border Padding (px):",
            "ja_JA": "外枠パディング (px)："
        },
        "Force Power of Two (2^n)": {
            "fr_FR": "Forcer la Puissance de Deux (2^n)",
            "en_US": "Force Power of Two (2^n)",
            "ja_JA": "2の累乗サイズに強制 (2^n)"
        },
        "Ensures atlas dimensions are powers of 2 for GPU hardware compatibility": {
            "fr_FR": "Garantit des dimensions en puissance de 2 pour la compatibilité matérielle GPU",
            "en_US": "Ensures atlas dimensions are powers of 2 for GPU hardware compatibility",
            "ja_JA": "GPUハードウェア互換性のためにアトラス寸法を2の累乗に保ちます"
        },
        "Force Square (1:1)": {
            "fr_FR": "Forcer le format carré (1:1)",
            "en_US": "Force Square (1:1)",
            "ja_JA": "正方形に強制 (1:1)"
        },
        "Deduplicate Identical Frames": {
            "fr_FR": "Dédupliquer les frames identiques",
            "en_US": "Deduplicate Identical Frames",
            "ja_JA": "同一フレームの重複排除"
        },
        "Identical frames share texture space in the atlas while preserving animation sequences": {
            "fr_FR": "Les frames identiques partagent le même espace de texture tout en préservant les séquences d'animation",
            "en_US": "Identical frames share texture space in the atlas while preserving animation sequences",
            "ja_JA": "同一フレームはテクスチャ空間を共有し、アニメーション順序を保持します"
        },
        "Trim Transparent Borders": {
            "fr_FR": "Rogner les bordures transparentes (Trim)",
            "en_US": "Trim Transparent Borders",
            "ja_JA": "透明な境界線をトリミング"
        },
        "Packing Statistics (Live Estimation)": {
            "fr_FR": "Statistiques d'Empaquetage (Évaluation directe)",
            "en_US": "Packing Statistics (Live Estimation)",
            "ja_JA": "パッキング統計（リアルタイム推定）"
        },
        "Dimensions: --": {
            "fr_FR": "Dimensions : --",
            "en_US": "Dimensions: --",
            "ja_JA": "寸法：--"
        },
        "Packing Efficiency: --": {
            "fr_FR": "Efficacité d'empaquetage : --",
            "en_US": "Packing Efficiency: --",
            "ja_JA": "充填効率：--"
        },
        "Frames: --": {
            "fr_FR": "Frames : --",
            "en_US": "Frames: --",
            "ja_JA": "フレーム：--"
        },
        "Export": {
            "fr_FR": "Exporter",
            "en_US": "Export",
            "ja_JA": "エクスポート"
        },
        "Godot 4 Resource (*.tres);;All Files (*.*)": {
            "fr_FR": "Ressource Godot 4 (*.tres);;Tous les fichiers (*.*)",
            "en_US": "Godot 4 Resource (*.tres);;All Files (*.*)",
            "ja_JA": "Godot 4 リソース (*.tres);;すべてのファイル (*.*)"
        },
        "JSON SpriteSheet (*.json);;All Files (*.*)": {
            "fr_FR": "SpriteSheet JSON (*.json);;Tous les fichiers (*.*)",
            "en_US": "JSON SpriteSheet (*.json);;All Files (*.*)",
            "ja_JA": "JSON スプライトシート (*.json);;すべてのファイル (*.*)"
        },
        "Select Export Destination": {
            "fr_FR": "Sélectionner la destination d'exportation",
            "en_US": "Select Export Destination",
            "ja_JA": "エクスポート先の選択"
        },
        "Frames: 0": {
            "fr_FR": "Frames : 0",
            "en_US": "Frames: 0",
            "ja_JA": "フレーム：0"
        },
        "Dimensions: %1 x %2 px (Current Atlas)": {
            "fr_FR": "Dimensions : %1 x %2 px (Atlas actuel)",
            "en_US": "Dimensions: %1 x %2 px (Current Atlas)",
            "ja_JA": "寸法：%1 x %2 px (現在のアトラス)"
        },
        "Packing Efficiency: Preserved as-is (WYSIWYG)": {
            "fr_FR": "Efficacité d'empaquetage : Conservée telle quelle (WYSIWYG)",
            "en_US": "Packing Efficiency: Preserved as-is (WYSIWYG)",
            "ja_JA": "充填効率：そのまま維持 (WYSIWYG)"
        },
        "Frames: %1 total": {
            "fr_FR": "Frames : %1 au total",
            "en_US": "Frames: %1 total",
            "ja_JA": "フレーム：合計 %1"
        },
        "Dimensions: No current atlas": {
            "fr_FR": "Dimensions : Aucun atlas actuel",
            "en_US": "Dimensions: No current atlas",
            "ja_JA": "寸法：現在のアトラスがありません"
        },
        "Dimensions: %1 x %2 px": {
            "fr_FR": "Dimensions : %1 x %2 px",
            "en_US": "Dimensions: %1 x %2 px",
            "ja_JA": "寸法：%1 x %2 px"
        },
        "Packing Efficiency: %1%": {
            "fr_FR": "Efficacité d'empaquetage : %1%",
            "en_US": "Packing Efficiency: %1%",
            "ja_JA": "充填効率：%1%"
        },
        "Frames: %1 total (%2 unique, %3 duplicates saved)": {
            "fr_FR": "Frames : %1 au total (%2 uniques, %3 doublons économisés)",
            "en_US": "Frames: %1 total (%2 unique, %3 duplicates saved)",
            "ja_JA": "フレーム：合計 %1 (固有 %2、節約重複 %3)"
        },
        "Frames: %1 total (all unique)": {
            "fr_FR": "Frames : %1 au total (toutes uniques)",
            "en_US": "Frames: %1 total (all unique)",
            "ja_JA": "フレーム：合計 %1 (すべて固有)"
        },
        "Dimensions: Does not fit in maximum bounds!": {
            "fr_FR": "Dimensions : Dépasse les limites maximales !",
            "en_US": "Dimensions: Does not fit in maximum bounds!",
            "ja_JA": "寸法：最大境界に収まりません！"
        },
        "Packing Efficiency: 0%": {
            "fr_FR": "Efficacité d'empaquetage : 0%",
            "en_US": "Packing Efficiency: 0%",
            "ja_JA": "充填効率：0%"
        },
        "Frames: %1": {
            "fr_FR": "Frames : %1",
            "en_US": "Frames: %1",
            "ja_JA": "フレーム：%1"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        }
    },
    "SettingsDialog": {
        "OK": {
            "fr_FR": "OK",
            "en_US": "OK",
            "ja_JA": "OK"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        },
        "Apply": {
            "fr_FR": "Appliquer",
            "en_US": "Apply",
            "ja_JA": "適用"
        },
        "Restore Defaults": {
            "fr_FR": "Restaurer les valeurs par défaut",
            "en_US": "Restore Defaults",
            "ja_JA": "デフォルトに戻す"
        }
    },
    "jsonExtractorDialog": {
        "Export": {
            "fr_FR": "Exporter",
            "en_US": "Export",
            "ja_JA": "エクスポート"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        }
    },
    "QPlatformTheme": {
        "OK": {
            "fr_FR": "OK",
            "en_US": "OK",
            "ja_JA": "OK"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        },
        "&Cancel": {
            "fr_FR": "&Annuler",
            "en_US": "&Cancel",
            "ja_JA": "キャンセル(&C)"
        },
        "Discard": {
            "fr_FR": "Ne pas enregistrer",
            "en_US": "Discard",
            "ja_JA": "破棄"
        },
        "&Discard": {
            "fr_FR": "&Ne pas enregistrer",
            "en_US": "&Discard",
            "ja_JA": "破棄(&D)"
        },
        "Save": {
            "fr_FR": "Enregistrer",
            "en_US": "Save",
            "ja_JA": "保存"
        },
        "&Save": {
            "fr_FR": "&Enregistrer",
            "en_US": "&Save",
            "ja_JA": "保存(&S)"
        },
        "Don't Save": {
            "fr_FR": "Ne pas enregistrer",
            "en_US": "Don't Save",
            "ja_JA": "保存しない"
        },
        "Apply": {
            "fr_FR": "Appliquer",
            "en_US": "Apply",
            "ja_JA": "適用"
        },
        "&Apply": {
            "fr_FR": "&Appliquer",
            "en_US": "&Apply",
            "ja_JA": "適用(&A)"
        },
        "Reset": {
            "fr_FR": "Réinitialiser",
            "en_US": "Reset",
            "ja_JA": "リセット"
        },
        "&Reset": {
            "fr_FR": "&Réinitialiser",
            "en_US": "&Reset",
            "ja_JA": "リセット(&R)"
        },
        "Restore Defaults": {
            "fr_FR": "Restaurer les valeurs par défaut",
            "en_US": "Restore Defaults",
            "ja_JA": "デフォルトに戻す"
        },
        "&Yes": {
            "fr_FR": "&Oui",
            "en_US": "&Yes",
            "ja_JA": "はい(&Y)"
        },
        "Yes": {
            "fr_FR": "Oui",
            "en_US": "Yes",
            "ja_JA": "はい"
        },
        "&No": {
            "fr_FR": "&Non",
            "en_US": "&No",
            "ja_JA": "いいえ(&N)"
        },
        "No": {
            "fr_FR": "Non",
            "en_US": "No",
            "ja_JA": "いいえ"
        },
        "Close": {
            "fr_FR": "Fermer",
            "en_US": "Close",
            "ja_JA": "閉じる"
        },
        "&Close": {
            "fr_FR": "&Fermer",
            "en_US": "&Close",
            "ja_JA": "閉じる(&C)"
        },
        "Open": {
            "fr_FR": "Ouvrir",
            "en_US": "Open",
            "ja_JA": "開く"
        },
        "&Open": {
            "fr_FR": "&Ouvrir",
            "en_US": "&Open",
            "ja_JA": "開く(&O)"
        }
    },
    "QDialogButtonBox": {
        "OK": {
            "fr_FR": "OK",
            "en_US": "OK",
            "ja_JA": "OK"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        },
        "&Cancel": {
            "fr_FR": "&Annuler",
            "en_US": "&Cancel",
            "ja_JA": "キャンセル(&C)"
        },
        "Discard": {
            "fr_FR": "Ne pas enregistrer",
            "en_US": "Discard",
            "ja_JA": "破棄"
        },
        "&Discard": {
            "fr_FR": "&Ne pas enregistrer",
            "en_US": "&Discard",
            "ja_JA": "破棄(&D)"
        },
        "Save": {
            "fr_FR": "Enregistrer",
            "en_US": "Save",
            "ja_JA": "保存"
        },
        "&Save": {
            "fr_FR": "&Enregistrer",
            "en_US": "&Save",
            "ja_JA": "保存(&S)"
        },
        "Don't Save": {
            "fr_FR": "Ne pas enregistrer",
            "en_US": "Don't Save",
            "ja_JA": "保存しない"
        },
        "Apply": {
            "fr_FR": "Appliquer",
            "en_US": "Apply",
            "ja_JA": "適用"
        },
        "&Apply": {
            "fr_FR": "&Appliquer",
            "en_US": "&Apply",
            "ja_JA": "適用(&A)"
        },
        "Reset": {
            "fr_FR": "Réinitialiser",
            "en_US": "Reset",
            "ja_JA": "リセット"
        },
        "&Reset": {
            "fr_FR": "&Réinitialiser",
            "en_US": "&Reset",
            "ja_JA": "リセット(&R)"
        },
        "Restore Defaults": {
            "fr_FR": "Restaurer les valeurs par défaut",
            "en_US": "Restore Defaults",
            "ja_JA": "デフォルトに戻す"
        },
        "&Yes": {
            "fr_FR": "&Oui",
            "en_US": "&Yes",
            "ja_JA": "はい(&Y)"
        },
        "Yes": {
            "fr_FR": "Oui",
            "en_US": "Yes",
            "ja_JA": "はい"
        },
        "&No": {
            "fr_FR": "&Non",
            "en_US": "&No",
            "ja_JA": "いいえ(&N)"
        },
        "No": {
            "fr_FR": "Non",
            "en_US": "No",
            "ja_JA": "いいえ"
        },
        "Close": {
            "fr_FR": "Fermer",
            "en_US": "Close",
            "ja_JA": "閉じる"
        },
        "&Close": {
            "fr_FR": "&Fermer",
            "en_US": "&Close",
            "ja_JA": "閉じる(&C)"
        },
        "Open": {
            "fr_FR": "Ouvrir",
            "en_US": "Open",
            "ja_JA": "開く"
        },
        "&Open": {
            "fr_FR": "&Ouvrir",
            "en_US": "&Open",
            "ja_JA": "開く(&O)"
        }
    },
    "QMessageBox": {
        "OK": {
            "fr_FR": "OK",
            "en_US": "OK",
            "ja_JA": "OK"
        },
        "Cancel": {
            "fr_FR": "Annuler",
            "en_US": "Cancel",
            "ja_JA": "キャンセル"
        },
        "&Cancel": {
            "fr_FR": "&Annuler",
            "en_US": "&Cancel",
            "ja_JA": "キャンセル(&C)"
        },
        "Discard": {
            "fr_FR": "Ne pas enregistrer",
            "en_US": "Discard",
            "ja_JA": "破棄"
        },
        "&Discard": {
            "fr_FR": "&Ne pas enregistrer",
            "en_US": "&Discard",
            "ja_JA": "破棄(&D)"
        },
        "Save": {
            "fr_FR": "Enregistrer",
            "en_US": "Save",
            "ja_JA": "保存"
        },
        "&Save": {
            "fr_FR": "&Enregistrer",
            "en_US": "&Save",
            "ja_JA": "保存(&S)"
        },
        "Don't Save": {
            "fr_FR": "Ne pas enregistrer",
            "en_US": "Don't Save",
            "ja_JA": "保存しない"
        },
        "Apply": {
            "fr_FR": "Appliquer",
            "en_US": "Apply",
            "ja_JA": "適用"
        },
        "&Apply": {
            "fr_FR": "&Appliquer",
            "en_US": "&Apply",
            "ja_JA": "適用(&A)"
        },
        "Reset": {
            "fr_FR": "Réinitialiser",
            "en_US": "Reset",
            "ja_JA": "リセット"
        },
        "&Reset": {
            "fr_FR": "&Réinitialiser",
            "en_US": "&Reset",
            "ja_JA": "リセット(&R)"
        },
        "Restore Defaults": {
            "fr_FR": "Restaurer les valeurs par défaut",
            "en_US": "Restore Defaults",
            "ja_JA": "デフォルトに戻す"
        },
        "&Yes": {
            "fr_FR": "&Oui",
            "en_US": "&Yes",
            "ja_JA": "はい(&Y)"
        },
        "Yes": {
            "fr_FR": "Oui",
            "en_US": "Yes",
            "ja_JA": "はい"
        },
        "&No": {
            "fr_FR": "&Non",
            "en_US": "&No",
            "ja_JA": "いいえ(&N)"
        },
        "No": {
            "fr_FR": "Non",
            "en_US": "No",
            "ja_JA": "いいえ"
        },
        "Close": {
            "fr_FR": "Fermer",
            "en_US": "Close",
            "ja_JA": "閉じる"
        },
        "&Close": {
            "fr_FR": "&Fermer",
            "en_US": "&Close",
            "ja_JA": "閉じる(&C)"
        },
        "Open": {
            "fr_FR": "Ouvrir",
            "en_US": "Open",
            "ja_JA": "開く"
        },
        "&Open": {
            "fr_FR": "&Ouvrir",
            "en_US": "&Open",
            "ja_JA": "開く(&O)"
        }
    }
}

# Alias for namespaced context
TRANSLATIONS["SpriteStudioWidgets::PolygonMeshDialog"] = TRANSLATIONS["PolygonMeshDialog"]

def update_file(lang):
    filepath = f"SpriteStudio/i18n/sprite_studio_{lang}.ts"
    tree = ET.parse(filepath)
    root = tree.getroot()

    updated = 0
    missing = []

    # Ensure all contexts from TRANSLATIONS exist in root
    existing_contexts = {ctx.find("name").text: ctx for ctx in root.findall("context") if ctx.find("name") is not None}
    for cname in TRANSLATIONS:
        if cname not in existing_contexts:
            new_ctx = ET.SubElement(root, "context")
            name_elem = ET.SubElement(new_ctx, "name")
            name_elem.text = cname
            existing_contexts[cname] = new_ctx

    for ctx in root.findall("context"):
        cname = ctx.find("name").text or "Global"
        ctx_trans = TRANSLATIONS.get(cname, {})

        existing_sources = set()
        for msg in ctx.findall("message"):
            src_elem = msg.find("source")
            if src_elem is None or not src_elem.text:
                continue
            src = src_elem.text
            existing_sources.add(src)
            tr_elem = msg.find("translation")

            if src in ctx_trans:
                text = ctx_trans[src].get(lang)
                if text is not None:
                    if tr_elem is None:
                        tr_elem = ET.SubElement(msg, "translation")
                    tr_elem.text = text
                    if "type" in tr_elem.attrib:
                        del tr_elem.attrib["type"]
                    updated += 1
            elif tr_elem is not None and (tr_elem.get("type") == "unfinished" or not tr_elem.text):
                missing.append((cname, src))

        # Add any sources from TRANSLATIONS that don't exist yet in ctx
        for src, trans_dict in ctx_trans.items():
            if src not in existing_sources:
                text = trans_dict.get(lang)
                if text is not None:
                    msg = ET.SubElement(ctx, "message")
                    src_elem = ET.SubElement(msg, "source")
                    src_elem.text = src
                    tr_elem = ET.SubElement(msg, "translation")
                    tr_elem.text = text
                    updated += 1

    tree.write(filepath, encoding="utf-8", xml_declaration=True)
    print(f"[{lang}] Updated {updated} translations. Remaining unfinished: {len(missing)}")
    if missing:
        for c, s in missing[:10]:
            print(f"   Missing: [{c}] {s.encode('ascii', 'backslashreplace').decode('ascii')}")

if __name__ == "__main__":
    for l in ["fr_FR", "en_US", "ja_JA"]:
        update_file(l)

    # Compile .ts into .qm with lrelease
    import subprocess
    lrelease_paths = [
        r"C:\Qt\6.10.2\mingw_64\bin\lrelease.exe",
        "lrelease"
    ]
    lrel = next((p for p in lrelease_paths if os.path.exists(p)), None)
    if lrel:
        for l in ["fr_FR", "en_US", "ja_JA"]:
            ts_path = f"SpriteStudio/i18n/sprite_studio_{l}.ts"
            qm_path = f"SpriteStudio/i18n/sprite_studio_{l}.qm"
            subprocess.run([lrel, ts_path, "-qm", qm_path], check=True)
            print(f"[{l}] Compiled binary QM: {qm_path}")
