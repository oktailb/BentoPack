@tool
class_name BentoI18n
extends RefCounted

## Zero-dependency i18n translation system for BentoPack Godot 4 Editor Addon.
## Supports English, French, Japanese, Simplified Chinese, Korean, Brazilian Portuguese, Spanish, and German.
## Automatically detects Godot Editor / OS locale, with manual override support.

enum Language {
	AUTO = 0,
	ENGLISH = 1,
	FRENCH = 2,
	JAPANESE = 3,
	CHINESE = 4,
	KOREAN = 5,
	PORTUGUESE = 6,
	SPANISH = 7,
	GERMAN = 8
}

const SETTINGS_FILE := "user://bentopack_editor_settings.cfg"
const SETTING_SECTION := "general"
const SETTING_KEY := "language"

static var _current_lang: Language = Language.AUTO
static var _initialized: bool = false

static func get_current_language() -> Language:
	if not _initialized:
		_load_settings()
	return _current_lang

static func set_current_language(lang: Language) -> void:
	_current_lang = lang
	_initialized = true
	_save_settings()

static func _load_settings() -> void:
	_initialized = true
	var cfg := ConfigFile.new()
	var err := cfg.load(SETTINGS_FILE)
	if err == OK:
		_current_lang = cfg.get_value(SETTING_SECTION, SETTING_KEY, Language.AUTO) as Language
	else:
		_current_lang = Language.AUTO

static func _save_settings() -> void:
	var cfg := ConfigFile.new()
	cfg.load(SETTINGS_FILE) # Load existing settings if any
	cfg.set_value(SETTING_SECTION, SETTING_KEY, int(_current_lang))
	cfg.save(SETTINGS_FILE)

static func get_active_language_code() -> String:
	match get_current_language():
		Language.ENGLISH: return "en"
		Language.FRENCH: return "fr"
		Language.JAPANESE: return "ja"
		Language.CHINESE: return "zh"
		Language.KOREAN: return "ko"
		Language.PORTUGUESE: return "pt"
		Language.SPANISH: return "es"
		Language.GERMAN: return "de"
		_:
			# Auto-detect from Godot Editor or OS locale
			var locale := ""
			if Engine.is_editor_hint() and EditorInterface != null:
				var ed_settings := EditorInterface.get_editor_settings()
				if ed_settings != null and ed_settings.has_setting("interface/editor/editor_language"):
					locale = str(ed_settings.get_setting("interface/editor/editor_language"))

			if locale.is_empty():
				locale = OS.get_locale()
			if locale.is_empty():
				locale = TranslationServer.get_locale()

			var loc_lower := locale.to_lower()
			if loc_lower.begins_with("fr"): return "fr"
			if loc_lower.begins_with("ja"): return "ja"
			if loc_lower.begins_with("zh"): return "zh"
			if loc_lower.begins_with("ko"): return "ko"
			if loc_lower.begins_with("pt"): return "pt"
			if loc_lower.begins_with("es"): return "es"
			if loc_lower.begins_with("de"): return "de"
			return "en"

static func translate(key: String, default_val: String = "") -> String:
	var lang := get_active_language_code()
	if TRANSLATIONS.has(key):
		var dict: Dictionary = TRANSLATIONS[key]
		if dict.has(lang):
			return dict[lang]
		if dict.has("en"):
			return dict["en"]
	return default_val if not default_val.is_empty() else key

static func t(key: String, default_val: String = "") -> String:
	return translate(key, default_val)

static func create_language_selector(on_change: Callable = Callable()) -> OptionButton:
	var opt := OptionButton.new()
	var labels: Array[String] = [
		"🌐 Auto",
		"🇬🇧 EN",
		"🇫🇷 FR",
		"🇯🇵 JA",
		"🇨🇳 中文",
		"🇰🇷 한국어",
		"🇧🇷 PT-BR",
		"🇪🇸 ES",
		"🇩🇪 DE"
	]
	var values: Array[Language] = [
		Language.AUTO,
		Language.ENGLISH,
		Language.FRENCH,
		Language.JAPANESE,
		Language.CHINESE,
		Language.KOREAN,
		Language.PORTUGUESE,
		Language.SPANISH,
		Language.GERMAN
	]

	var cur_lang := get_current_language()
	var select_idx := 0
	for i in range(values.size()):
		opt.add_item(labels[i], values[i])
		if values[i] == cur_lang:
			select_idx = i

	opt.selected = select_idx
	opt.item_selected.connect(func(idx: int):
		var new_val := values[idx]
		set_current_language(new_val)
		if on_change.is_valid():
			on_change.call(new_val)
	)
	return opt

const TRANSLATIONS: Dictionary = {
	"dock_title": {
		"en": "BentoPack Pipeline & Watcher",
		"fr": "Pipeline & Surveillance BentoPack",
		"ja": "BentoPack パイプライン＆監視",
		"zh": "BentoPack 流水线与监听器",
		"ko": "BentoPack 파이프라인 및 감시자",
		"pt": "Pipeline e Monitor BentoPack",
		"es": "Pipeline y Monitor BentoPack",
		"de": "BentoPack Pipeline & Watcher"
	},
	"cli_checking": {
		"en": "Checking CLI...",
		"fr": "Vérification du CLI...",
		"ja": "CLI を確認中...",
		"zh": "正在检查 CLI...",
		"ko": "CLI 확인 중...",
		"pt": "Verificando CLI...",
		"es": "Comprobando CLI...",
		"de": "CLI wird überprüft..."
	},
	"cli_not_found": {
		"en": "CLI: Not detected (PATH or ProjectSettings)",
		"fr": "CLI : Non détecté (PATH ou ProjectSettings)",
		"ja": "CLI : 未検出 (PATH または ProjectSettings)",
		"zh": "CLI : 未检测到 (PATH 或 ProjectSettings)",
		"ko": "CLI : 감지되지 않음 (PATH 또는 ProjectSettings)",
		"pt": "CLI : Não detectado (PATH ou ProjectSettings)",
		"es": "CLI : No detectado (PATH o ProjectSettings)",
		"de": "CLI : Nicht erkannt (PATH oder ProjectSettings)"
	},
	"cli_ok": {
		"en": "CLI: %s (OK)",
		"fr": "CLI : %s (OK)",
		"ja": "CLI : %s (正常)",
		"zh": "CLI : %s (正常)",
		"ko": "CLI : %s (정상)",
		"pt": "CLI : %s (OK)",
		"es": "CLI : %s (OK)",
		"de": "CLI : %s (OK)"
	},
	"download_cli": {
		"en": "📥 Download CLI...",
		"fr": "📥 Télécharger CLI...",
		"ja": "📥 CLI をダウンロード...",
		"zh": "📥 下载 CLI...",
		"ko": "📥 CLI 다운로드...",
		"pt": "📥 Baixar CLI...",
		"es": "📥 Descargar CLI...",
		"de": "📥 CLI herunterladen..."
	},
	"download_cli_tooltip": {
		"en": "Open the official GitHub Releases page to download precompiled binaries for your system.",
		"fr": "Ouvrir la page officielle des Releases GitHub pour télécharger les binaires précompilés pour votre système.",
		"ja": "公式 GitHub Releases ページを開き、お使いのシステム用の事前ビルド済みバイナリをダウンロードします。",
		"zh": "打开 GitHub Releases 官方页面下载适用于您系统的预编译二进制文件。",
		"ko": "공식 GitHub Releases 페이지를 열어 시스템에 맞는 사전 컴파일된 바이너리를 다운로드합니다.",
		"pt": "Abrir a página oficial de lançamentos do GitHub para baixar os binários pré-compilados para o seu sistema.",
		"es": "Abrir la página oficial de Releases en GitHub para descargar los binarios precompilados para su sistema.",
		"de": "Offizielle GitHub-Releases-Seite öffnen, um vorkompilierte Binärdateien für Ihr System herunterzuladen."
	},
	"open_gui": {
		"en": "Open BentoPack Studio",
		"fr": "Ouvrir BentoPack GUI",
		"ja": "BentoPack Studio を開く",
		"zh": "打开 BentoPack Studio",
		"ko": "BentoPack Studio 열기",
		"pt": "Abrir BentoPack Studio",
		"es": "Abrir BentoPack Studio",
		"de": "BentoPack Studio öffnen"
	},
	"source_atlas": {
		"en": "Source Atlas (Input Image):",
		"fr": "Atlas source (Image Entrée) :",
		"ja": "ソースアトラス (入力画像) :",
		"zh": "源图集 (输入图像) :",
		"ko": "소스 아틀라스 (입력 이미지) :",
		"pt": "Atlas de Origem (Imagem de Entrada):",
		"es": "Atlas de Origen (Imagen de Entrada):",
		"de": "Quell-Atlas (Eingabebild):"
	},
	"source_atlas_tooltip": {
		"en": "PNG/WebP image file or folder containing sprite frames.",
		"fr": "Fichier image PNG/WebP ou dossier contenant les frames de sprites.",
		"ja": "PNG/WebP 画像ファイル、またはスプライトフレームを含むフォルダー。",
		"zh": "PNG/WebP 图像文件或包含精灵帧的文件夹。",
		"ko": "PNG/WebP 이미지 파일 또는 스프라이트 프레임이 포함된 폴더입니다.",
		"pt": "Arquivo de imagem PNG/WebP ou pasta contendo os quadros de sprite.",
		"es": "Archivo de imagen PNG/WebP o carpeta que contiene los fotogramas de sprites.",
		"de": "PNG/WebP-Bilddatei oder Ordner mit Sprite-Frames."
	},
	"source_placeholder": {
		"en": "res://spritesheet.png or frames folder",
		"fr": "res://spritesheet.png ou dossier de frames",
		"ja": "res://spritesheet.png またはフレームフォルダー",
		"zh": "res://spritesheet.png 或帧序列文件夹",
		"ko": "res://spritesheet.png 또는 프레임 폴더",
		"pt": "res://spritesheet.png ou pasta de quadros",
		"es": "res://spritesheet.png o carpeta de fotogramas",
		"de": "res://spritesheet.png oder Frames-Ordner"
	},
	"browse": {
		"en": "Browse...",
		"fr": "Parcourir...",
		"ja": "参照...",
		"zh": "浏览...",
		"ko": "찾아보기...",
		"pt": "Procurar...",
		"es": "Examinar...",
		"de": "Durchsuchen..."
	},
	"target_project": {
		"en": "Project File (Output .bento):",
		"fr": "Fichier projet (Sortie .bento) :",
		"ja": "プロジェクトファイル (出力 .bento) :",
		"zh": "项目文件 (输出 .bento) :",
		"ko": "프로젝트 파일 (출력 .bento) :",
		"pt": "Arquivo de Projeto (Saída .bento):",
		"es": "Archivo de Proyecto (Salida .bento):",
		"de": "Projektdatei (Ausgabe .bento):"
	},
	"target_project_tooltip": {
		"en": ".bento project file generated by CLI, automatically imported into Godot with animations and polygonal mesh.",
		"fr": "Fichier projet .bento généré par le CLI et automatiquement importé dans Godot avec les animations et le mesh polygonal.",
		"ja": "CLI によって生成され、アニメーションとポリゴンメッシュを含めて Godot に自動インポートされる .bento ファイル。",
		"zh": "由 CLI 生成的 .bento 项目文件，将连同动画和多边形网格自动导入 Godot。",
		"ko": "CLI에서 생성되어 애니메이션 및 폴리곤 메시와 함께 Godot으로 자동 임포트되는 .bento 프로젝트 파일입니다.",
		"pt": "Arquivo de projeto .bento gerado pela CLI e importado automaticamente para o Godot com animações e malha poligonal.",
		"es": "Archivo de proyecto .bento generado por la CLI e importado automáticamente en Godot con animaciones y malla poligonal.",
		"de": ".bento-Projektdatei, die von der CLI generiert und automatisch mit Animationen und Polygon-Mesh in Godot importiert wird."
	},
	"slice_label": {
		"en": "Slicing:",
		"fr": "Découpage :",
		"ja": "スライス方式 :",
		"zh": "切片方式 :",
		"ko": "슬라이스 방식 :",
		"pt": "Fatiamento:",
		"es": "Recorte:",
		"de": "Zuschnitt:"
	},
	"slice_mode_auto": {
		"en": "Auto-Slice (Transparency / Silhouettes)",
		"fr": "Auto-Slice (Transparence / Silhouettes)",
		"ja": "自動スライス (透明度 / 輪郭検出)",
		"zh": "自动切片 (透明度 / 轮廓检测)",
		"ko": "자동 슬라이스 (투명도 / 실루엣)",
		"pt": "Auto-Fatiamento (Transparência / Silhuetas)",
		"es": "Auto-Recorte (Transparencia / Siluetas)",
		"de": "Auto-Zuschnitt (Transparenz / Silhouetten)"
	},
	"slice_mode_grid": {
		"en": "Regular Grid (Tiles)",
		"fr": "Grille régulière (Tuiles)",
		"ja": "等間隔グリッド (タイル)",
		"zh": "规则网格 (瓦片)",
		"ko": "일반 그리드 (타일)",
		"pt": "Grade Regular (Ladrilhos)",
		"es": "Cuadrícula Regular (Teselas)",
		"de": "Reguläres Gitter (Kacheln)"
	},
	"slice_mode_files": {
		"en": "Individual Images (Folder)",
		"fr": "Images individuelles (Dossier)",
		"ja": "個別画像 (フォルダー)",
		"zh": "独立图像 (文件夹)",
		"ko": "개별 이미지 (폴더)",
		"pt": "Imagens Individuais (Pasta)",
		"es": "Imágenes Individuales (Carpeta)",
		"de": "Einzelne Bilder (Ordner)"
	},
	"packing_label": {
		"en": "Packing:",
		"fr": "Packing :",
		"ja": "パッキング方式 :",
		"zh": "打包算法 :",
		"ko": "패킹 방식 :",
		"pt": "Empacotamento:",
		"es": "Empaquetado:",
		"de": "Packen:"
	},
	"algo_maxrects": {
		"en": "MaxRects (Rectangular)",
		"fr": "MaxRects (Rectangulaire)",
		"ja": "MaxRects (矩形)",
		"zh": "MaxRects (矩形)",
		"ko": "MaxRects (직사각형)",
		"pt": "MaxRects (Retangular)",
		"es": "MaxRects (Rectangular)",
		"de": "MaxRects (Rechteckig)"
	},
	"algo_m8": {
		"en": "Polygonal M8 (Tight Mesh)",
		"fr": "Polygonal M8 (Tight Mesh)",
		"ja": "ポリゴナル M8 (タイトメッシュ)",
		"zh": "多边形 M8 (紧密网格)",
		"ko": "폴리곤 M8 (밀착 메시)",
		"pt": "Poligonal M8 (Malha Justa)",
		"es": "Poligonal M8 (Malla Ajustada)",
		"de": "Polygonal M8 (Enges Mesh)"
	},
	"watcher_mode": {
		"en": "Watcher Mode (--watch background)",
		"fr": "Mode Watcher (--watch background)",
		"ja": "監視モード (--watch バックグラウンド)",
		"zh": "监听模式 (--watch 后台)",
		"ko": "감시 모드 (--watch 백그라운드)",
		"pt": "Modo Monitor (--watch em segundo plano)",
		"es": "Modo Monitor (--watch en segundo plano)",
		"de": "Watcher-Modus (--watch im Hintergrund)"
	},
	"watcher_tooltip": {
		"en": "Watches source image for modifications and automatically re-generates animations on every save.",
		"fr": "Surveille les modifications de l'image source et re-génère les animations automatiquement à chaque sauvegarde.",
		"ja": "ソース画像の変更を監視し、保存されるたびにアニメーションを自動的に再生成します。",
		"zh": "监听源图像的修改，并在每次保存时自动重新生成动画。",
		"ko": "소스 이미지 변경 사항을 감시하여 저장할 때마다 자동으로 애니메이션을 재생성합니다.",
		"pt": "Monitora alterações na imagem de origem e recria as animações automaticamente a cada salvamento.",
		"es": "Supervisa cambios en la imagen de origen y regenera las animaciones automáticamente al guardar.",
		"de": "Überwacht das Quellbild auf Änderungen und generiert Animationen bei jedem Speichern automatisch neu."
	},
	"process_btn": {
		"en": "⚡ Process & Generate Animations",
		"fr": "⚡ Traiter & Générer Animations",
		"ja": "⚡ 処理してアニメーションを生成",
		"zh": "⚡ 处理并生成动画",
		"ko": "⚡ 처리 및 애니메이션 생성",
		"pt": "⚡ Processar e Gerar Animações",
		"es": "⚡ Procesar y Generar Animaciones",
		"de": "⚡ Verarbeiten & Animationen generieren"
	},
	"stop_watcher": {
		"en": "Stop Watcher",
		"fr": "Arrêter Watcher",
		"ja": "監視を停止",
		"zh": "停止监听",
		"ko": "감시자 중지",
		"pt": "Parar Monitor",
		"es": "Detener Monitor",
		"de": "Watcher stoppen"
	},
	"status_ready": {
		"en": "Ready.",
		"fr": "Prêt.",
		"ja": "準備完了。",
		"zh": "就绪。",
		"ko": "준비 완료.",
		"pt": "Pronto.",
		"es": "Listo.",
		"de": "Bereit."
	},
	"status_processing": {
		"en": "CLI processing in progress...",
		"fr": "Traitement CLI en cours...",
		"ja": "CLI 処理を実行中...",
		"zh": "CLI 正在处理中...",
		"ko": "CLI 처리 진행 중...",
		"pt": "Processamento CLI em andamento...",
		"es": "Procesamiento CLI en curso...",
		"de": "CLI-Verarbeitung läuft..."
	},
	"status_watcher_active": {
		"en": "Watcher active (PID %d). Watching %s...",
		"fr": "Watcher actif (PID %d). Surveillance de %s...",
		"ja": "監視中 (PID %d)。%s を監視しています...",
		"zh": "监听器运行中 (PID %d)。正在监听 %s...",
		"ko": "감시자 활성 상태 (PID %d). %s 감시 중...",
		"pt": "Monitor ativo (PID %d). Monitorando %s...",
		"es": "Monitor activo (PID %d). Supervisando %s...",
		"de": "Watcher aktiv (PID %d). Überwacht %s..."
	},
	"status_watcher_failed": {
		"en": "Failed to launch watcher.",
		"fr": "Échec du lancement du watcher.",
		"ja": "監視の起動に失敗しました。",
		"zh": "启动监听器失败。",
		"ko": "감시자 실행에 실패했습니다.",
		"pt": "Falha ao iniciar o monitor.",
		"es": "Error al iniciar el monitor.",
		"de": "Starten des Watchers fehlgeschlagen."
	},
	"status_watcher_stopped": {
		"en": "Watcher stopped.",
		"fr": "Watcher arrêté.",
		"ja": "監視を停止しました。",
		"zh": "监听器已停止。",
		"ko": "감시자가 중지되었습니다.",
		"pt": "Monitor parado.",
		"es": "Monitor detenido.",
		"de": "Watcher gestoppt."
	},
	"status_no_watcher": {
		"en": "No active watcher.",
		"fr": "Aucun watcher actif.",
		"ja": "実行中の監視はありません。",
		"zh": "无运行中的监听器。",
		"ko": "활성 감시자 없음.",
		"pt": "Nenhum monitor ativo.",
		"es": "Ningún monitor activo.",
		"de": "Kein aktiver Watcher."
	},
	"status_success": {
		"en": "Success! %s generated and ready in Godot.",
		"fr": "Succès ! %s généré et prêt dans Godot.",
		"ja": "成功！%s が生成され、Godot で使用可能です。",
		"zh": "成功！%s 已生成并可在 Godot 中使用。",
		"ko": "성공! %s이(가) 생성되어 Godot에서 사용할 준비가 되었습니다.",
		"pt": "Sucesso! %s gerado e pronto no Godot.",
		"es": "¡Éxito! %s generado y listo en Godot.",
		"de": "Erfolg! %s generiert und einsatzbereit in Godot."
	},
	"status_error": {
		"en": "CLI Error (code %d): %s",
		"fr": "Erreur CLI (code %d) : %s",
		"ja": "CLI エラー (コード %d) : %s",
		"zh": "CLI 错误 (代码 %d) : %s",
		"ko": "CLI 오류 (코드 %d) : %s",
		"pt": "Erro CLI (código %d): %s",
		"es": "Error CLI (código %d): %s",
		"de": "CLI-Fehler (Code %d): %s"
	},
	"error_specify_source": {
		"en": "Error: Please specify a source image file.",
		"fr": "Erreur : Veuillez spécifier un fichier image source.",
		"ja": "エラー : ソース画像ファイルを指定してください。",
		"zh": "错误 : 请指定源图像文件。",
		"ko": "오류 : 소스 이미지 파일을 지정해 주세요.",
		"pt": "Erro: Especifique um arquivo de imagem de origem.",
		"es": "Error: Especifique un archivo de imagen de origen.",
		"de": "Fehler: Bitte geben Sie eine Quellbilddatei an."
	},
	"error_cli_not_found": {
		"en": "Error: bentopack-cli not found.",
		"fr": "Erreur : bentopack-cli introuvable.",
		"ja": "エラー : bentopack-cli が見つかりません。",
		"zh": "错误 : 未找到 bentopack-cli。",
		"ko": "오류 : bentopack-cli를 찾을 수 없습니다.",
		"pt": "Erro: bentopack-cli não encontrado.",
		"es": "Error: bentopack-cli no encontrado.",
		"de": "Fehler: bentopack-cli nicht gefunden."
	},
	"ctx_auto_slice": {
		"en": "⚡ BentoPack: Auto-Slice & Generate Scene",
		"fr": "⚡ BentoPack : Auto-Slice & Générer Scène",
		"ja": "⚡ BentoPack : 自動スライス＆シーン生成",
		"zh": "⚡ BentoPack : 自动切片并生成场景",
		"ko": "⚡ BentoPack : 자동 슬라이스 및 씬 생성",
		"pt": "⚡ BentoPack : Auto-Fatiamento e Gerar Cena",
		"es": "⚡ BentoPack : Auto-Recorte y Generar Escena",
		"de": "⚡ BentoPack : Auto-Zuschnitt & Szene generieren"
	},
	"ctx_open_in_dock": {
		"en": "🔧 BentoPack: Configure in Dock",
		"fr": "🔧 BentoPack : Configurer dans le Dock",
		"ja": "🔧 BentoPack : ドックで設定",
		"zh": "🔧 BentoPack : 在底部面板中配置",
		"ko": "🔧 BentoPack : 독에서 설정",
		"pt": "🔧 BentoPack : Configurar no Painel",
		"es": "🔧 BentoPack : Configurar en el Panel",
		"de": "🔧 BentoPack : Im Dock konfigurieren"
	},
	"ctx_open_desktop": {
		"en": "🎨 BentoPack: Open in Desktop Editor",
		"fr": "🎨 BentoPack : Ouvrir dans l'éditeur Desktop",
		"ja": "🎨 BentoPack : デスクトップエディターで開く",
		"zh": "🎨 BentoPack : 在桌面编辑器中打开",
		"ko": "🎨 BentoPack : 데스크톱 편집기에서 열기",
		"pt": "🎨 BentoPack : Abrir no Editor Desktop",
		"es": "🎨 BentoPack : Abrir en el Editor de Escritorio",
		"de": "🎨 BentoPack : Im Desktop-Editor öffnen"
	},
	"inspector_title": {
		"en": "BentoPack 2D Toolkit",
		"fr": "Boîte à outils 2D BentoPack",
		"ja": "BentoPack 2D ツールキット",
		"zh": "BentoPack 2D 工具箱",
		"ko": "BentoPack 2D 툴킷",
		"pt": "Kit de Ferramentas 2D BentoPack",
		"es": "Kit de Herramientas 2D BentoPack",
		"de": "BentoPack 2D-Toolkit"
	},
	"inspector_open_btn": {
		"en": "🎨 Open in BentoPack",
		"fr": "🎨 Ouvrir dans BentoPack",
		"ja": "🎨 BentoPack で開く",
		"zh": "🎨 在 BentoPack 中打开",
		"ko": "🎨 BentoPack에서 열기",
		"pt": "🎨 Abrir no BentoPack",
		"es": "🎨 Abrir en BentoPack",
		"de": "🎨 In BentoPack öffnen"
	},
	"inspector_open_tooltip": {
		"en": "Launch the BentoPack desktop application to edit this asset.",
		"fr": "Lancer l'application BentoPack pour éditer cet asset.",
		"ja": "デスクトップアプリ BentoPack を起動してこのアセットを編集します。",
		"zh": "启动 BentoPack 桌面应用程序以编辑此资源。",
		"ko": "BentoPack 데스크톱 앱을 실행하여 이 에셋을 편집합니다.",
		"pt": "Iniciar o aplicativo BentoPack para editar este ativo.",
		"es": "Iniciar la aplicación BentoPack para editar este asset.",
		"de": "BentoPack Desktop-Anwendung starten, um dieses Asset zu bearbeiten."
	},
	"menu_open_editor": {
		"en": "BentoPack: Open Desktop Editor",
		"fr": "BentoPack : Ouvrir l'éditeur Desktop",
		"ja": "BentoPack : デスクトップエディターを開く",
		"zh": "BentoPack : 打开桌面编辑器",
		"ko": "BentoPack : 데스크톱 편집기 열기",
		"pt": "BentoPack : Abrir Editor Desktop",
		"es": "BentoPack : Abrir Editor de Escritorio",
		"de": "BentoPack : Desktop-Editor öffnen"
	},
	"menu_show_cli": {
		"en": "BentoPack: Show CLI Info",
		"fr": "BentoPack : Afficher les infos CLI",
		"ja": "BentoPack : CLI 情報を表示",
		"zh": "BentoPack : 显示 CLI 信息",
		"ko": "BentoPack : CLI 정보 표시",
		"pt": "BentoPack : Exibir Informações da CLI",
		"es": "BentoPack : Mostrar Información de la CLI",
		"de": "BentoPack : CLI-Info anzeigen"
	}
}
