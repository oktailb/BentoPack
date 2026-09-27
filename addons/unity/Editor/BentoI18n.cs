using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace BentoPack.Editor
{
    public enum BentoLanguage
    {
        Auto = 0,
        French = 1,
        English = 2,
        Japanese = 3,
        Chinese = 4,
        Korean = 5,
        Portuguese = 6,
        Spanish = 7,
        German = 8
    }

    /// <summary>
    /// Zero-dependency i18n translation system for BentoPack Unity Editor extensions.
    /// Supports English, French, Japanese, Simplified Chinese, Korean, Brazilian Portuguese, Spanish, and German,
    /// with auto-detection of Unity / system language.
    /// </summary>
    public static class BentoI18n
    {
        private const string PREF_KEY = "BentoPack_Editor_Language";
        private static BentoLanguage s_currentLang = BentoLanguage.Auto;
        private static bool s_initialized = false;

        public static BentoLanguage CurrentLanguage
        {
            get
            {
                if (!s_initialized) LoadLanguage();
                return s_currentLang;
            }
            set
            {
                s_currentLang = value;
                s_initialized = true;
                EditorPrefs.SetInt(PREF_KEY, (int)s_currentLang);
            }
        }

        public static void LoadLanguage()
        {
            s_currentLang = (BentoLanguage)EditorPrefs.GetInt(PREF_KEY, (int)BentoLanguage.Auto);
            s_initialized = true;
        }

        public static string GetActiveLanguageCode()
        {
            switch (CurrentLanguage)
            {
                case BentoLanguage.French: return "fr";
                case BentoLanguage.Japanese: return "ja";
                case BentoLanguage.English: return "en";
                case BentoLanguage.Chinese: return "zh";
                case BentoLanguage.Korean: return "ko";
                case BentoLanguage.Portuguese: return "pt";
                case BentoLanguage.Spanish: return "es";
                case BentoLanguage.German: return "de";
                default:
                    var sys = Application.systemLanguage;
                    if (sys == SystemLanguage.French) return "fr";
                    if (sys == SystemLanguage.Japanese) return "ja";
                    if (sys == SystemLanguage.Chinese || sys == SystemLanguage.ChineseSimplified || sys == SystemLanguage.ChineseTraditional) return "zh";
                    if (sys == SystemLanguage.Korean) return "ko";
                    if (sys == SystemLanguage.Portuguese) return "pt";
                    if (sys == SystemLanguage.Spanish) return "es";
                    if (sys == SystemLanguage.German) return "de";
                    return "en";
            }
        }

        public static string Tr(string key)
        {
            string lang = GetActiveLanguageCode();
            if (s_translations.TryGetValue(key, out var dict))
            {
                if (dict.TryGetValue(lang, out var str)) return str;
                if (dict.TryGetValue("en", out var enStr)) return enStr;
            }
            return key;
        }

        public static void DrawLanguageSelector()
        {
            string[] labels = new string[] {
                "🌐 Auto",
                "🇬🇧 EN",
                "🇫🇷 FR",
                "🇯🇵 JA",
                "🇨🇳 中文",
                "🇰🇷 한국어",
                "🇧🇷 PT-BR",
                "🇪🇸 ES",
                "🇩🇪 DE"
            };
            BentoLanguage[] values = new BentoLanguage[] {
                BentoLanguage.Auto,
                BentoLanguage.English,
                BentoLanguage.French,
                BentoLanguage.Japanese,
                BentoLanguage.Chinese,
                BentoLanguage.Korean,
                BentoLanguage.Portuguese,
                BentoLanguage.Spanish,
                BentoLanguage.German
            };

            int curIdx = 0;
            for (int i = 0; i < values.Length; ++i)
            {
                if (values[i] == CurrentLanguage) { curIdx = i; break; }
            }

            EditorGUI.BeginChangeCheck();
            int newIdx = EditorGUILayout.Popup(curIdx, labels, GUILayout.Width(82));
            if (EditorGUI.EndChangeCheck())
            {
                CurrentLanguage = values[newIdx];
                SceneView.RepaintAll();
            }
        }

        private static readonly Dictionary<string, Dictionary<string, string>> s_translations = new Dictionary<string, Dictionary<string, string>>
        {
            ["reimport"] = new Dictionary<string, string>
            {
                ["fr"] = "⚡ Réimporter & Synchroniser",
                ["en"] = "⚡ Reimport & Sync",
                ["ja"] = "⚡ 再インポート＆同期",
                ["zh"] = "⚡ 重新导入并同步",
                ["ko"] = "⚡ 다시 가져오기 및 동기화",
                ["pt"] = "⚡ Reimportar e Sincronizar",
                ["es"] = "⚡ Reimportar y Sincronizar",
                ["de"] = "⚡ Reimportieren & Synchronisieren"
            },
            ["unity_asset"] = new Dictionary<string, string>
            {
                ["fr"] = "📦 Asset Unity",
                ["en"] = "📦 Unity Asset",
                ["ja"] = "📦 Unity アセット",
                ["zh"] = "📦 Unity 资源",
                ["ko"] = "📦 Unity 에셋",
                ["pt"] = "📦 Ativo do Unity",
                ["es"] = "📦 Asset de Unity",
                ["de"] = "📦 Unity Asset"
            },
            ["external_source"] = new Dictionary<string, string>
            {
                ["fr"] = "🔗 Source externe (.bento)",
                ["en"] = "🔗 External Source (.bento)",
                ["ja"] = "🔗 外部ソース (.bento)",
                ["zh"] = "🔗 外部源文件 (.bento)",
                ["ko"] = "🔗 외부 소스 (.bento)",
                ["pt"] = "🔗 Fonte Externa (.bento)",
                ["es"] = "🔗 Fuente Externa (.bento)",
                ["de"] = "🔗 Externe Quelle (.bento)"
            },
            ["ext_linked"] = new Dictionary<string, string>
            {
                ["fr"] = "✓ Lié : ",
                ["en"] = "✓ Linked : ",
                ["ja"] = "✓ リンク済み : ",
                ["zh"] = "✓ 已关联 : ",
                ["ko"] = "✓ 연결됨 : ",
                ["pt"] = "✓ Vinculado : ",
                ["es"] = "✓ Vinculado : ",
                ["de"] = "✓ Verknüpft : "
            },
            ["ext_not_linked"] = new Dictionary<string, string>
            {
                ["fr"] = "💡 Aucun fichier externe relié (utilise la copie Assets/).",
                ["en"] = "💡 No external file linked (using internal Assets/ copy).",
                ["ja"] = "💡 外部ファイル未設定 (内部 Assets/ コピーを使用)。",
                ["zh"] = "💡 未关联外部文件（使用 Assets/ 内副本）。",
                ["ko"] = "💡 연결된 외부 파일 없음 (내부 Assets/ 복사본 사용).",
                ["pt"] = "💡 Nenhum arquivo externo vinculado (usando cópia interna Assets/).",
                ["es"] = "💡 Ningún archivo externo vinculado (usando copia interna Assets/).",
                ["de"] = "💡 Keine externe Datei verknüpft (interne Assets/-Kopie wird verwendet)."
            },
            ["controller_title"] = new Dictionary<string, string>
            {
                ["fr"] = "🍱 Contrôleur d'Animation 2D BentoPack",
                ["en"] = "🍱 BentoPack 2D Animation Controller",
                ["ja"] = "🍱 BentoPack 2D アニメーションコントローラー",
                ["zh"] = "🍱 BentoPack 2D 动画控制器",
                ["ko"] = "🍱 BentoPack 2D 애니메이션 컨트롤러",
                ["pt"] = "🍱 Controlador de Animação 2D BentoPack",
                ["es"] = "🍱 Controlador de Animación 2D BentoPack",
                ["de"] = "🍱 BentoPack 2D-Animations-Controller"
            },
            ["active_animation"] = new Dictionary<string, string>
            {
                ["fr"] = "Animation active",
                ["en"] = "Active Animation",
                ["ja"] = "アクティブアニメーション",
                ["zh"] = "当前动画",
                ["ko"] = "활성 애니메이션",
                ["pt"] = "Animação Ativa",
                ["es"] = "Animación Activa",
                ["de"] = "Aktive Animation"
            },
            ["play"] = new Dictionary<string, string>
            {
                ["fr"] = "▶ Lecture",
                ["en"] = "▶ Play",
                ["ja"] = "▶ 再生",
                ["zh"] = "▶ 播放",
                ["ko"] = "▶ 재생",
                ["pt"] = "▶ Reproduzir",
                ["es"] = "▶ Reproducir",
                ["de"] = "▶ Abspielen"
            },
            ["pause"] = new Dictionary<string, string>
            {
                ["fr"] = "⏸ Pause",
                ["en"] = "⏸ Pause",
                ["ja"] = "⏸ 一時停止",
                ["zh"] = "⏸ 暂停",
                ["ko"] = "⏸ 일시정지",
                ["pt"] = "⏸ Pausar",
                ["es"] = "⏸ Pausa",
                ["de"] = "⏸ Pause"
            },
            ["stop"] = new Dictionary<string, string>
            {
                ["fr"] = "⏹ Arrêt",
                ["en"] = "⏹ Stop",
                ["ja"] = "⏹ 停止",
                ["zh"] = "⏹ 停止",
                ["ko"] = "⏹ 정지",
                ["pt"] = "⏹ Parar",
                ["es"] = "⏹ Detener",
                ["de"] = "⏹ Stopp"
            },
            ["frame"] = new Dictionary<string, string>
            {
                ["fr"] = "Image (Frame)",
                ["en"] = "Frame",
                ["ja"] = "フレーム",
                ["zh"] = "帧 (Frame)",
                ["ko"] = "프레임 (Frame)",
                ["pt"] = "Quadro (Frame)",
                ["es"] = "Fotograma (Frame)",
                ["de"] = "Frame"
            },
            ["flip_x"] = new Dictionary<string, string>
            {
                ["fr"] = "Miroir X",
                ["en"] = "Flip X",
                ["ja"] = "左右反転",
                ["zh"] = "水平翻转",
                ["ko"] = "좌우 반전",
                ["pt"] = "Inverter X",
                ["es"] = "Voltear X",
                ["de"] = "X spiegeln"
            },
            ["flip_y"] = new Dictionary<string, string>
            {
                ["fr"] = "Miroir Y",
                ["en"] = "Flip Y",
                ["ja"] = "上下反転",
                ["zh"] = "垂直翻转",
                ["ko"] = "상하 반전",
                ["pt"] = "Inverter Y",
                ["es"] = "Voltear Y",
                ["de"] = "Y spiegeln"
            },
            ["extract_folders"] = new Dictionary<string, string>
            {
                ["fr"] = "📂 Extraire vers des dossiers",
                ["en"] = "📂 Extract to Folders",
                ["ja"] = "📂 フォルダーに展開",
                ["zh"] = "📂 解压至文件夹",
                ["ko"] = "📂 폴더로 추출",
                ["pt"] = "📂 Extrair para Pastas",
                ["es"] = "📂 Extraer a Carpetas",
                ["de"] = "📂 In Ordner entpacken"
            },
            ["open_studio"] = new Dictionary<string, string>
            {
                ["fr"] = "🎨 Ouvrir dans BentoPack",
                ["en"] = "🎨 Open in BentoPack",
                ["ja"] = "🎨 BentoPack で開く",
                ["zh"] = "🎨 在 BentoPack 中打开",
                ["ko"] = "🎨 BentoPack에서 열기",
                ["pt"] = "🎨 Abrir no BentoPack",
                ["es"] = "🎨 Abrir en BentoPack",
                ["de"] = "🎨 In BentoPack öffnen"
            },
            ["animations_group"] = new Dictionary<string, string>
            {
                ["fr"] = "🎬 Animations",
                ["en"] = "🎬 Animations",
                ["ja"] = "🎬 アニメーション",
                ["zh"] = "🎬 动画列表",
                ["ko"] = "🎬 애니메이션",
                ["pt"] = "🎬 Animações",
                ["es"] = "🎬 Animaciones",
                ["de"] = "🎬 Animationen"
            },
            ["frames_group"] = new Dictionary<string, string>
            {
                ["fr"] = "🖼️ Frames / Sprites",
                ["en"] = "🖼️ Frames / Sprites",
                ["ja"] = "🖼️ フレーム / スプライト",
                ["zh"] = "🖼️ 帧序列 / 精灵",
                ["ko"] = "🖼️ 프레임 / 스프라이트",
                ["pt"] = "🖼️ Quadros / Sprites",
                ["es"] = "🖼️ Fotogramas / Sprites",
                ["de"] = "🖼️ Frames / Sprites"
            },
            ["metadata_group"] = new Dictionary<string, string>
            {
                ["fr"] = "🗄️ Métadonnées & Atlas",
                ["en"] = "🗄️ Metadata & Atlas",
                ["ja"] = "🗄️ メタデータ & アトラス",
                ["zh"] = "🗄️ 元数据与图集",
                ["ko"] = "🗄️ 메타데이터 및 아틀라스",
                ["pt"] = "🗄️ Metadados e Atlas",
                ["es"] = "🗄️ Metadatos y Atlas",
                ["de"] = "🗄️ Metadaten & Atlas"
            },
            ["ground"] = new Dictionary<string, string>
            {
                ["fr"] = "Sol",
                ["en"] = "Ground",
                ["ja"] = "地面",
                ["zh"] = "地面",
                ["ko"] = "바닥",
                ["pt"] = "Chão",
                ["es"] = "Suelo",
                ["de"] = "Boden"
            },
            ["center"] = new Dictionary<string, string>
            {
                ["fr"] = "Centre",
                ["en"] = "Center",
                ["ja"] = "中央",
                ["zh"] = "中心",
                ["ko"] = "중심",
                ["pt"] = "Centro",
                ["es"] = "Centro",
                ["de"] = "Mitte"
            },
            ["mire"] = new Dictionary<string, string>
            {
                ["fr"] = "Mire",
                ["en"] = "Reticle",
                ["ja"] = "照準",
                ["zh"] = "准星",
                ["ko"] = "조준선",
                ["pt"] = "Retículo",
                ["es"] = "Retícula",
                ["de"] = "Fadenkreuz"
            },
            ["sync_success"] = new Dictionary<string, string>
            {
                ["fr"] = "✓ Réimporté et synchronisé avec succès !",
                ["en"] = "✓ Successfully reimported and synchronized!",
                ["ja"] = "✓ 再インポートと同期が完了しました！",
                ["zh"] = "✓ 重新导入并同步成功！",
                ["ko"] = "✓ 다시 가져오기 및 동기화 완료!",
                ["pt"] = "✓ Reimportado e sincronizado com sucesso!",
                ["es"] = "✓ ¡Reimportado y sincronizado con éxito!",
                ["de"] = "✓ Erfolgreich reimportiert und synchronisiert!"
            },
            ["hitbox_status"] = new Dictionary<string, string>
            {
                ["fr"] = "✓ PolygonCollider2D synchronisé : {0} sommets sur '{1}' (frame {2}/{3}).\nℹ️ Hitbox vert néon (6px) et mire rouge au sol dessinées en surimpression permanente dans la vue Scene.",
                ["en"] = "✓ PolygonCollider2D synchronized: {0} vertices on '{1}' (frame {2}/{3}).\nℹ️ Neon green hitbox (6px) and red ground reticle permanently drawn in Scene view.",
                ["ja"] = "✓ PolygonCollider2D 同期完了 : '{1}' で {0} 頂点 (フレーム {2}/{3})。\nℹ️ ネオングリーンの当たり判定 (6px) と地面の赤照準をSceneビューに常時描画。",
                ["zh"] = "✓ PolygonCollider2D 已同步 : '{1}' 上有 {0} 个顶点 (第 {2}/{3} 帧)。\nℹ️ Scene 视图中常驻显示霓虹绿碰撞盒 (6px) 与地面红色准星。",
                ["ko"] = "✓ PolygonCollider2D 동기화 완료 : '{1}'에 {0}개 정점 (프레임 {2}/{3}).\nℹ️ 네온 그린 히트박스(6px)와 바닥 빨간 조준선이 Scene 뷰에 항상 표시됩니다.",
                ["pt"] = "✓ PolygonCollider2D sincronizado : {0} vértices em '{1}' (quadro {2}/{3}).\nℹ️ Hitbox verde neon (6px) e retículo vermelho no chão desenhados permanentemente na visualização Scene.",
                ["es"] = "✓ PolygonCollider2D sincronizado: {0} vértices en '{1}' (fotograma {2}/{3}).\nℹ️ Hitbox verde neón (6px) y retícula roja en el suelo dibujadas permanentemente en la vista Scene.",
                ["de"] = "✓ PolygonCollider2D synchronisiert: {0} Scheitelpunkte auf '{1}' (Frame {2}/{3}).\nℹ️ Neongrüne Hitbox (6px) und rotes Boden-Fadenkreuz dauerhaft in der Scene-Ansicht gezeichnet."
            },
            ["microscopic_warning"] = new Dictionary<string, string>
            {
                ["fr"] = "⚠️ Les sommets actuels du PolygonCollider2D sont microscopiques.\nCliquez ci-dessous pour forcer la réimportation automatique :",
                ["en"] = "⚠️ Current PolygonCollider2D vertices are microscopic.\nClick below to force automatic reimport:",
                ["ja"] = "⚠️ PolygonCollider2D の頂点が極小です。\n下のボタンをクリックして自動再インポートを実行してください：",
                ["zh"] = "⚠️ 当前 PolygonCollider2D 顶点过小（微米级）。\n请点击下方按钮强制自动重新导入：",
                ["ko"] = "⚠️ 현재 PolygonCollider2D 정점이 너무 작습니다.\n아래 버튼을 눌러 자동 다시 가져오기를 실행하세요:",
                ["pt"] = "⚠️ Os vértices atuais do PolygonCollider2D são microscópicos.\nClique abaixo para forçar a reimportação automática:",
                ["es"] = "⚠️ Los vértices actuales de PolygonCollider2D son microscópicos.\nHaga clic a continuación para forzar la reimportación automática:",
                ["de"] = "⚠️ Die aktuellen Scheitelpunkte des PolygonCollider2D sind mikroskopisch klein.\nKlicken Sie unten, um den automatischen Reimport zu erzwingen:"
            },
            ["reimport_auto"] = new Dictionary<string, string>
            {
                ["fr"] = "🔄 Réimporter automatiquement l'asset .bento",
                ["en"] = "🔄 Automatically reimport .bento asset",
                ["ja"] = "🔄 .bento アセットを自動再インポート",
                ["zh"] = "🔄 自动重新导入 .bento 资源",
                ["ko"] = "🔄 .bento 에셋 자동 다시 가져오기",
                ["pt"] = "🔄 Reimportar automaticamente o arquivo .bento",
                ["es"] = "🔄 Reimportar automáticamente el asset .bento",
                ["de"] = "🔄 .bento-Asset automatisch reimportieren"
            },
            ["advanced_properties"] = new Dictionary<string, string>
            {
                ["fr"] = "⚙️ Propriétés avancées du composant",
                ["en"] = "⚙️ Advanced Component Properties",
                ["ja"] = "⚙️ コンポーネントの詳細プロパティ",
                ["zh"] = "⚙️ 组件高级属性",
                ["ko"] = "⚙️ 컴포넌트 고급 속성",
                ["pt"] = "⚙️ Propriedades Avançadas do Componente",
                ["es"] = "⚙️ Propiedades Avanzadas del Componente",
                ["de"] = "⚙️ Erweiterte Komponenteneigenschaften"
            },
            ["multi_selected"] = new Dictionary<string, string>
            {
                ["fr"] = "🍱 {0} personnages BentoPack sélectionnés simultanément.\nSélectionnez un seul personnage pour contrôler ses animations et ses frames individuelles.",
                ["en"] = "🍱 {0} BentoPack characters selected simultaneously.\nSelect a single character to control its animations and individual frames.",
                ["ja"] = "🍱 {0} 体の BentoPack キャラクターが同時に選択されています。\nアニメーションや個別フレームを操作するには、1体のみ選択してください。",
                ["zh"] = "🍱 同时选中了 {0} 个 BentoPack 角色。\n如需控制单个角色的动画和帧，请仅选择一个角色。",
                ["ko"] = "🍱 {0}개의 BentoPack 캐릭터가 동시에 선택되었습니다.\n애니메이션과 프레임을 개별 제어하려면 캐릭터 하나만 선택하세요.",
                ["pt"] = "🍱 {0} personagens BentoPack selecionados simultaneamente.\nSelecione apenas um personagem para controlar animações e quadros individuais.",
                ["es"] = "🍱 {0} personajes BentoPack seleccionados simultáneamente.\nSeleccione un solo personaje para controlar animaciones y fotogramas individuales.",
                ["de"] = "🍱 {0} BentoPack-Charaktere gleichzeitig ausgewählt.\nWählen Sie einen einzelnen Charakter aus, um dessen Animationen und einzelne Frames zu steuern."
            },
            ["pick_source_title"] = new Dictionary<string, string>
            {
                ["fr"] = "Sélectionner le fichier .bento externe",
                ["en"] = "Select external .bento file",
                ["ja"] = "外部の .bento ファイルを選択",
                ["zh"] = "选择外部 .bento 文件",
                ["ko"] = "외부 .bento 파일 선택",
                ["pt"] = "Selecionar arquivo .bento externo",
                ["es"] = "Seleccionar archivo .bento externo",
                ["de"] = "Externe .bento-Datei auswählen"
            },
            ["file_not_found"] = new Dictionary<string, string>
            {
                ["fr"] = "Fichier externe introuvable : {0}",
                ["en"] = "External file not found: {0}",
                ["ja"] = "外部ファイルが見つかりません : {0}",
                ["zh"] = "未找到外部文件 : {0}",
                ["ko"] = "외부 파일을 찾을 수 없습니다: {0}",
                ["pt"] = "Arquivo externo não encontrado: {0}",
                ["es"] = "Archivo externo no encontrado: {0}",
                ["de"] = "Externe Datei nicht gefunden: {0}"
            },
            ["no_bento_asset"] = new Dictionary<string, string>
            {
                ["fr"] = "(aucun asset .bento lié)",
                ["en"] = "(no .bento asset linked)",
                ["ja"] = "(リンクされた .bento アセットなし)",
                ["zh"] = "(未关联 .bento 资源)",
                ["ko"] = "(연결된 .bento 에셋 없음)",
                ["pt"] = "(nenhum ativo .bento vinculado)",
                ["es"] = "(ningún asset .bento vinculado)",
                ["de"] = "(kein .bento-Asset verknüpft)"
            },
            ["content_summary"] = new Dictionary<string, string>
            {
                ["fr"] = "Contenu : {0} animations | {1} frames | Atlas {2}",
                ["en"] = "Content: {0} animations | {1} frames | Atlas {2}",
                ["ja"] = "コンテンツ : {0} アニメーション | {1} フレーム | アトラス {2}",
                ["zh"] = "内容 : {0} 个动画 | {1} 帧 | 图集 {2}",
                ["ko"] = "콘텐츠 : 애니메이션 {0}개 | 프레임 {1}개 | 아틀라스 {2}",
                ["pt"] = "Conteúdo : {0} animações | {1} quadros | Atlas {2}",
                ["es"] = "Contenido : {0} animaciones | {1} fotogramas | Atlas {2}",
                ["de"] = "Inhalt : {0} Animationen | {1} Frames | Atlas {2}"
            },
            ["no_animations"] = new Dictionary<string, string>
            {
                ["fr"] = "Aucune animation générée.",
                ["en"] = "No animations generated.",
                ["ja"] = "生成されたアニメーションはありません。",
                ["zh"] = "未生成动画。",
                ["ko"] = "생성된 애니메이션 없음.",
                ["pt"] = "Nenhuma animação gerada.",
                ["es"] = "Ninguna animación generada.",
                ["de"] = "Keine Animationen generiert."
            },
            ["inspect"] = new Dictionary<string, string>
            {
                ["fr"] = "Inspecter",
                ["en"] = "Inspect",
                ["ja"] = "検査",
                ["zh"] = "检查",
                ["ko"] = "검사",
                ["pt"] = "Inspecionar",
                ["es"] = "Inspeccionar",
                ["de"] = "Prüfen"
            },
            ["more_frames"] = new Dictionary<string, string>
            {
                ["fr"] = "... et {0} autres frames.",
                ["en"] = "... and {0} more frames.",
                ["ja"] = "... 他 {0} フレーム。",
                ["zh"] = "... 以及其他 {0} 帧。",
                ["ko"] = "... 외 {0}개 프레임.",
                ["pt"] = "... e mais {0} quadros.",
                ["es"] = "... y {0} fotogramas más.",
                ["de"] = "... und {0} weitere Frames."
            },
            ["texture_atlas_info"] = new Dictionary<string, string>
            {
                ["fr"] = "Texture Atlas : {0} × {1} px (Format : {2})",
                ["en"] = "Atlas Texture: {0} × {1} px (Format: {2})",
                ["ja"] = "アトラステクスチャ : {0} × {1} px (フォーマット : {2})",
                ["zh"] = "图集纹理 : {0} × {1} px (格式 : {2})",
                ["ko"] = "아틀라스 텍스처 : {0} × {1} px (포맷 : {2})",
                ["pt"] = "Textura do Atlas : {0} × {1} px (Formato : {2})",
                ["es"] = "Textura del Atlas : {0} × {1} px (Formato : {2})",
                ["de"] = "Atlas-Textur : {0} × {1} px (Format : {2})"
            },
            ["import_settings"] = new Dictionary<string, string>
            {
                ["fr"] = "Paramètres d'importation",
                ["en"] = "Import Settings",
                ["ja"] = "インポート設定",
                ["zh"] = "导入设置",
                ["ko"] = "가져오기 설정",
                ["pt"] = "Configurações de Importação",
                ["es"] = "Ajustes de Importación",
                ["de"] = "Importeinstellungen"
            },
            ["generate_animation_clips"] = new Dictionary<string, string>
            {
                ["fr"] = "Générer AnimationClips",
                ["en"] = "Generate AnimationClips",
                ["ja"] = "AnimationClip を生成",
                ["zh"] = "生成 AnimationClip",
                ["ko"] = "AnimationClip 생성",
                ["pt"] = "Gerar AnimationClips",
                ["es"] = "Generar AnimationClips",
                ["de"] = "AnimationClips generieren"
            },
            ["tight_mesh_label"] = new Dictionary<string, string>
            {
                ["fr"] = "Maillage polygonal ajusté (M8)",
                ["en"] = "Tight Polygonal Mesh (M8)",
                ["ja"] = "タイトポリゴンメッシュ (M8)",
                ["zh"] = "紧密多边形网格 (M8)",
                ["ko"] = "밀착 폴리곤 메시 (M8)",
                ["pt"] = "Malha Poligonal Justa (M8)",
                ["es"] = "Malla Poligonal Ajustada (M8)",
                ["de"] = "Eng anliegendes Polygon-Mesh (M8)"
            }
        };
    }
}
