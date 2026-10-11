#pragma once

/*
==================================================================================
                                资源模型与识别level_pe.cpp
==================================================================================
*/

/* ===================== 原RcCore.h ===================== */
#include <map>
#include <iostream>
#include <vector>
#include <utility>
#include <memory>
#include <string>
#include <initializer_list>

#include "../../utils/TextConversion.h"
#include "../../utils/ResourceUtils.h"

namespace rceditor
{

    // 全局标志（之前定义在 LevelManagerGlobals.cpp）
    extern bool gbIsLevelAdmin;               // 管理员
    extern unsigned short gbCurNeedShowLan;   // 界面语言索引


#pragma region 全局函数与全局常量

    const std::map<CStringW, WORD> MAP_CONTROL_NAME_WORD =
	{
		// 标准 Windows 控件（大写是Windows API标准）
		{L"BUTTON", 0x80},
		{L"EDIT", 0x81},
		{L"STATIC", 0x82},
		{L"LISTBOX", 0x83},
		{L"SCROLLBAR", 0x84},
		{L"COMBOBOX", 0x85},
		{L"MDICLIENT", 0x86},
		{L"COMBOLBOX", 0x87},
		{L"DDEMLEEVENT", 0x88},

		// 工具栏和通用控件
		{L"TOOLBARWINDOW32", 0x8F},
		{L"MSCTLS_STATUSBAR32", 0x90},
		{L"MSCTLS_PROGRESS32", 0x91},
		{L"MSCTLS_TRACKBAR32", 0x92},
		{L"MSCTLS_UPDOWN32", 0x93},
		{L"MSCTLS_HOTKEY32", 0x94},
		{L"SYSHEADER32", 0x95},
		{L"SYSLISTVIEW32", 0x96},
		{L"SYSTREEVIEW32", 0x97},
		{L"SYSTABCONTROL32", 0x98},
		{L"SYSANIMATE32", 0x99},
		{L"SYSDATETIMEPICK32", 0x9A},
		{L"SYSMONTHCAL32", 0x9B},
		{L"TOOLTIPS_CLASS32", 0x9C},
		{L"REBARWINDOW32", 0x9D},
		{L"MSCTLS_TOOLBAR32", 0x9E},
		{L"SYSLINK", 0x9F},

		// RichEdit 控件
		{L"RICHEDIT", 0xA0},
		{L"RichEdit20W", 0xA1},
		{L"RichEdit50W", 0xA2},

		// 其他控件
		{L"SysIPAddress32", 0xA3},
		{L"SysPager", 0xA4},
		{L"NativeFontCtl", 0xA5},
		{L"DirectUIHWND", 0xA6},
		{L"Windows.UI.Core.CoreWindow", 0xA7},

		// 兼容性 - 常用控件的小写版本
		{L"button", 0x80},
		{L"edit", 0x81},
		{L"static", 0x82},
		{L"listbox", 0x83},
		{L"scrollbar", 0x84},
		{L"combobox", 0x85},

		// 兼容性 - 混合大小写版本（资源编辑器可能使用）
		{L"Button", 0x80},
		{L"Edit", 0x81},
		{L"Static", 0x82},
		{L"ListBox", 0x83},
		{L"ScrollBar", 0x84},
		{L"ComboBox", 0x85}
	};




#pragma endregion 全局函数与全局常量

#pragma region PE信息头
	struct SConvertedResData
	{
		LPCTSTR resType;			/* 资源类型 */
		UINT nResID;				/* 该语言下所有的当前资源类型的资源ID */
		LANGID langID;				/* 当前资源类型所属的语言 */
		std::vector<BYTE> vecData; /* 资源数据 */

		explicit SConvertedResData()
		{
			resType = RT_DIALOG;
			nResID = 0;
			langID = 0;
			vecData.clear();
		}
	};

	//! 每种语言的资源ID
	struct SPerLanResID
	{
		LANGID langID;	/* 当前资源类型所属的语言 */
		std::vector<int> vecResID;	/* 该语言下所有的当前资源类型的资源ID */
		explicit SPerLanResID()
		{
			langID = 0;
			vecResID.clear();
		}
	};

#pragma pack(push, 1)
	struct DLGTEMPLATEEX
	{
		WORD dlgVer;
		WORD signature; // 必须是 0xFFFF
		DWORD helpID;
		DWORD dwExtendedStyle;
		DWORD style;
		WORD cDlgItems;
		short x;
		short y;
		short cx;
		short cy;
		// 后面跟着菜单、类名、标题、字体
	};

	struct DLGITEMTEMPLATEEX
	{
		DWORD helpID;
		DWORD dwExtendedStyle;
		DWORD style;
		short x;
		short y;
		short cx;
		short cy;
		DWORD id;
		// 后面跟着类名、标题、extra data
	};

	// 扩展菜单头
	struct MENUEX_TEMPLATE_HEADER
	{
		WORD wVersion;    // = 1
		WORD wOffset;     // = 4
		DWORD dwHelpId;
	};

	// 扩展菜单项
	struct MENUEX_TEMPLATE_ITEM
	{
		DWORD dwType;
		DWORD dwState;
		DWORD dwHelpId;
		WORD  wID;        // 菜单项ID
		WORD  bResInfo;   // Popup标志和子项数量
		// WCHAR szText[]; 后面是菜单文本（以 0 结尾）
	};

	//! toolbar资源头
	struct TOOLBAR_TEMPLATE
	{
		uint16_t wVersion;   /* 版本，通常为 1 */
		uint16_t wWidth;     /* 按钮宽度 */
		uint16_t wHeight;    /* 按钮高度 */
		uint16_t wItemCount; /* 按钮数量 */
		// 后面跟着 wItemCount 个 uint16_t 的按钮 ID
	};


#pragma pack(pop)

#pragma endregion PE信息头


#pragma region 中间转储
	//! 字体信息
	struct SFontInfo
	{
		CStringW csFaceName;		//! 字体名称
		WORD wPointSize;			/* 字体大小 */
		WORD wWeight;				/* 字体粗细 */
		BYTE bItalic;				/* 是否斜体 0 ==> 否 1 ==> 是 */
		BYTE bCharSet;				/* 字符集 */
	public:
		explicit SFontInfo()
		{
			wPointSize = 0;
			wWeight = 0;
			bItalic = 0;
			bCharSet = 0;
			csFaceName.Empty();
		}


		/*!
		 * \brief 将字体信息添加到资源数据中
		 * \param resource 资源数据缓冲区
		 * \param offset 当前偏移位置
		 * \param isEx 是否为扩展对话框模板
		 * \return 添加字体信息后的新偏移位置
		 */
		size_t AddFontInfoToResource(std::vector<BYTE>& resource, size_t offset, bool isEx) const;
	};


	//! 控件信息
	struct SCtrlInfo
	{
		//! 资源自带的信息
		DWORD dwStyle; /* 控件风格 */
		DWORD dwExStyle; /* 控件扩展风格 */
		CRect ctrlRect; /* 控件位置 */
		UINT nIDC; /* 控件ID */
		CStringW csClassName; /* 控件类名 */
		CStringW csText; /* 控件文本 */
		SFontInfo oFontInfo; /* 控件字体信息 */
		WORD wResourceID; /* 控件资源ID */
		bool blIsResourceIDText; /* 控件资源ID文本标志  true ==> 文本为资源ID false ==> 文本不是资源ID */
		std::vector<std::shared_ptr<BYTE>> vecExtraData; /* 控件的额外数据 */
		bool blMultiLineEditCtrl; /* 是否为多行编辑控件 true ==> 是 false ==> 否 */


		//! 非资源信息
		constexpr static short MAX_LEVEL_USE = 32; /* 最大的使用信息记录数 若需提升为64，需要改变当前json为nlohmann/json.hpp */

		UINT uiLevelMask;		/* 当前等级是否可禁用， 用位数表达，比如第0位为1表示 1级可使用 */
		void SetLevel(const int& nLevel, const bool& bEnable);
		bool GetLevel(const int& nLevel) const;
		// 记录新增信息的成员
		bool m_blIsAdded; /* 控件是否为新增项 */


	public:
		explicit SCtrlInfo()
		{
			dwStyle = (WS_CHILD | WS_VISIBLE);
			dwExStyle = 0;
			ctrlRect = CRect{ 10,10,10,10 };
			nIDC = 0;
			csClassName = L"STATIC";
			csText = L"";
			oFontInfo = SFontInfo();
			wResourceID = 0;
			blIsResourceIDText = false;
			vecExtraData.clear();
			blMultiLineEditCtrl = false;
			uiLevelMask = 0;
			m_blIsAdded = false;
		}
		SCtrlInfo(const SCtrlInfo& other);

		/*!
		 * \brief 将控件信息添加到资源数据中
		 * \param resource 资源数据缓冲区
		 * \param offset 当前偏移位置
		 * \param isEx 是否为扩展对话框模板
		 * \return 添加控件信息后的新偏移位置
		 */
		size_t AddControlToResource(std::vector<BYTE>& resource, size_t offset, bool isEx) const;
	};

	//! 窗口信息
	struct SDialogInfo
	{
		DWORD dwStyle; /* 窗口风格 */
		DWORD dwExStyle; /* 窗口扩展风格 */
		CRect rectDlg; /* 窗口位置 */
		UINT nIDD; /* 窗口ID */
		LANGID langID; /* 窗口所属的语言ID */
		CStringW csClassName; /* 窗口类名 */
		bool bIsDlgTemplateEx; /* 是否为DLGTEMPLATEEX 是 ==> DLGTEMPLATEEX 否 ==> DLGTEMPLATE 默认为false  */
		CStringW csCaption; /* 窗口标题 */
		CStringW csMenuName; /* 窗口菜单名 */
		WORD wMenuResourceID; /* 窗口菜单资源ID */
		bool blIsMenuResourceIDText; /* 窗口菜单资源ID文本标志  true ==> 文本为资源ID false ==> 文本不是资源ID */
		WORD wClassResourceID; /* 窗口类资源ID */
		bool blIsClassResourceIDText; /* 窗口类资源ID文本标志  true ==> 文本为资源ID false ==> 文本不是资源ID */
		std::shared_ptr<SFontInfo> pFontInfo; /* 窗口字体信息 */
		std::vector<std::shared_ptr<SCtrlInfo>> vecCtrlInfo; /* 窗口控件信息 */




		//! 非PE头信息
		bool m_blIsInLevelManager; /* 是否在等级管理器中 true ==> 是 false ==> 否 */
		CStringW csClassPath; /* 动态运行时类名路径（LevelManager配置使用，非资源字段） */
		// 记录新增控件信息 (以控件ID为key便于查找)
        std::map<UINT, std::shared_ptr<SCtrlInfo>> mapAddedCtrlInfo; /* 新增的控件信息，Key = nIDC */

        // 动态窗口标志：由运行时代码（如 CLevelManagerControler）创建的动态窗口
        // 这些窗口在 JSON 中以 `DynamicWndInfos` 单独存储（与静态 WndInfos 并列）
        bool m_blIsDynamic;

	public:
		explicit SDialogInfo()
		{
			dwStyle = 0;
			dwExStyle = 0;
			rectDlg = CRect{ 10,10,10,10 };
			nIDD = 0;
			langID = 1033;
			csClassName.Empty();
			csClassPath.Empty();
			bIsDlgTemplateEx = false;
			csCaption.Empty();
			csMenuName.Empty();
			wMenuResourceID = 0;
			blIsMenuResourceIDText = false;
			wClassResourceID = 0;
			blIsClassResourceIDText = false;
			pFontInfo = nullptr;
			vecCtrlInfo.clear();
			mapAddedCtrlInfo.clear();
			m_blIsInLevelManager = false;
			m_blIsDynamic = false;
		}

		//! 将窗口信息转为PE信息头
		SConvertedResData Convert2ResData() const;

		SDialogInfo& operator+=(const SDialogInfo& other);
		SDialogInfo& operator-=(const SDialogInfo& other);
		SDialogInfo operator+(const SDialogInfo& other) const;
		SDialogInfo operator-(const SDialogInfo& other) const;


	private:
		//! 计算窗口信息在PE头中的大小
		size_t CalculateDialogResourceSize() const;
	};

	//! stringtable信息
	struct SStringTableInfo
	{
		UINT nResourceStringID; /* 字符串资源块ID */

		//! 一般最多16个字符串	
        std::map<UINT, CStringW> mapIDAndString; /* 字符串ID和字符串文本的映射 */
		std::vector<UINT> vecLackStringID; /* 缺失的字符串ID -- 用于次语言版本 */
		// 记录新增的字符串 (以字符串ID为key便于查找)
        std::map<UINT, CStringW> mapAddedStrings; /* 新增的字符串，Key = string ID */
		explicit SStringTableInfo()
		{
			nResourceStringID = 0;
			mapIDAndString.clear();
			vecLackStringID.clear();
			mapAddedStrings.clear();
		}
		SStringTableInfo& operator+=(const SStringTableInfo& other);
		SStringTableInfo& operator-=(const SStringTableInfo& other);
		SStringTableInfo operator+(const SStringTableInfo& other) const;
		SStringTableInfo operator-(const SStringTableInfo& other) const;

		//! 将字符串表信息转为PE信息头
		SConvertedResData Convert2ResData(LANGID langID) const;

	};

	//! version信息
	struct SVersionInfo
	{
		LANGID langID; /* 语言ID */
		UINT nVersionID; /* 版本资源ID */

		WORD wCodePage; /* 代码页 */

		VS_FIXEDFILEINFO fileInfo; /* 固定文件信息 */
        std::map<CStringW, CStringW> mapStringInfo; /* 字符串信息映射 key ==> 键值 value ==> 键值对应的文本 */


	public:
		explicit SVersionInfo()
		{
			langID = 0;
			nVersionID = 0;
			wCodePage = 0;
			mapStringInfo.clear();
			memset(&fileInfo, 0, sizeof(VS_FIXEDFILEINFO));
		}

		//! 将version信息转为PE信息头
		SConvertedResData Convert2ResData() const;

		SVersionInfo& operator+=(const SVersionInfo& other);
		SVersionInfo& operator-=(const SVersionInfo& other);
		SVersionInfo operator+(const SVersionInfo& other) const;
		SVersionInfo operator-(const SVersionInfo& other) const;

	private:
		//! 添加字符串（含 DWORD 对齐）
		size_t AddVersionString(std::vector<BYTE>& data, size_t offset, const CStringW& str) const;
		//! 写入 VERSION 节头（wLength 可先写 0，后续回填）
		size_t WriteVersionHeader(std::vector<BYTE>& data, size_t offset, WORD wLength, WORD wValueLength, WORD wType) const;
	};
	//! 菜单信息
	struct SMenuInfo
	{
		LANGID lanID;	/* 所属语言ID */
		UINT nID; /* 菜单ID */
		CStringW csText; /* 菜单文本 */
		std::vector<std::shared_ptr<SMenuInfo>> vecSubMenu; /* 子菜单 */
		bool blCurMenuLacked; /* 当前菜单是否为缺失 true ==> 缺失 false ==> 不缺失 仅为比对使用 */
		DWORD nType;                        /* 菜单项类型 (MENUEX用) */
		DWORD nState;                       /* 菜单项状态 (MENUEX用) */
		DWORD dwHelpID;                     /* 帮助ID (MENUEX用) */
		bool bIsPopup;						/* 是否为 POPUP 子菜单 */
	public:
		explicit SMenuInfo()
		{
			nID = 0;
			lanID = 1033;
			csText.Empty();
			vecSubMenu.clear();
			nType = 0;
			nState = 0;
			dwHelpID = 0;
			bIsPopup = false;
			blCurMenuLacked = false;
		}

		//! 将菜单信息转为PE信息头
		SConvertedResData Convert2ResData() const;

		SMenuInfo& operator+=(const SMenuInfo& other);
		SMenuInfo& operator-=(const SMenuInfo& other);
		SMenuInfo operator+(const SMenuInfo& other) const;
		SMenuInfo operator-(const SMenuInfo& other) const;

	private:
		//! 递归添加菜单项到资源数据
		static size_t AddMenuItemToResource(std::vector<BYTE>& data, size_t offset, const SMenuInfo& menuItem);
	};
	//! toolbar信息
	struct SToolbarInfo
	{
		LANGID	langID; /* 语言ID */
		int nToolbarID; /* 工具栏资源ID */

		//! toolbar的具体数据
		uint16_t wVersion; /* 版本 */
		uint16_t wWidth; /* 宽度 */
		uint16_t wHeight; /* 高度 */
		std::vector<uint16_t> vecItemIDs; /* 工具栏按钮ID */

		// Per-button level masks for LevelManager support (runtime editable)
		std::vector<UINT> vecLevelMask; /* per-button level mask */
        // Whether this toolbar participates in the Level Manager (analogous to SDialogInfo::m_blIsInLevelManager)
        bool m_blIsInLevelManager;
        // Optional per-button text/descriptions (populated from external JSON)
        std::vector<CStringW> vecItemText; /* per-button text descriptions */
        // Optional bitmap handle representing toolbar image strip (created when parsing PE)
        HBITMAP hBitmap;

		explicit SToolbarInfo()
		{
			langID = 0;
			nToolbarID = 0;
			wVersion = 0;
			wWidth = 0;
			wHeight = 0;
			vecItemIDs.clear();
			vecLevelMask.clear();
			m_blIsInLevelManager = false;
			vecItemText.clear();
			hBitmap = nullptr;
		}

		~SToolbarInfo()
		{
			if (hBitmap)
			{
				::DeleteObject(hBitmap);
				hBitmap = nullptr;
			}
		}

		//! 将toolbar信息转为PE信息头
		SConvertedResData Convert2ResData() const;

		SToolbarInfo& operator+=(const SToolbarInfo& other);
		SToolbarInfo& operator-=(const SToolbarInfo& other);
		SToolbarInfo operator+(const SToolbarInfo& other) const;
		SToolbarInfo operator-(const SToolbarInfo& other) const;

	};


	//! 加速表 -- 不需要


	//! 其他 - 待更新



	//! 单个封装的资源信息
	struct SSingleResourceInfo
	{
		LANGID langID; /* 语言ID */
		//! 窗口信息
        std::map<UINT, std::shared_ptr<SDialogInfo>> mapLangAndDialogInfo; /* Dialog资源ID -> 窗口信息 */

		//! 字符串表信息
        std::map<UINT, std::shared_ptr<SStringTableInfo>> mapLangAndStringTableInfo; /* StringTable资源块ID -> 字符串表信息 */

		//! 版本信息
		std::shared_ptr<SVersionInfo> pVersionInfo; /* 版本信息 */
		//! 菜单信息
        std::map<UINT, std::shared_ptr<SMenuInfo>> mapLangAndMenuInfo; /* Menu资源ID -> 菜单信息 */

		//! 工具栏信息
        std::map<UINT, std::shared_ptr<SToolbarInfo>> mapLangAndToolbarInfo; /* Toolbar资源ID -> 工具栏信息 */
        //! 动态窗口信息（运行时注册的窗口）
        std::map<UINT, std::shared_ptr<SDialogInfo>> mapDynamicDialogInfo; /* Dynamic dialog ID -> 窗口信息 */

		explicit SSingleResourceInfo()
		{
			langID = 0;
			mapLangAndDialogInfo.clear();
			mapLangAndStringTableInfo.clear();
			pVersionInfo = nullptr;
			mapLangAndMenuInfo.clear();
            mapDynamicDialogInfo.clear();
			mapLangAndToolbarInfo.clear();
		}

		//! 重载运算符
		SSingleResourceInfo& operator+=(const SSingleResourceInfo& other);
		SSingleResourceInfo& operator-=(const SSingleResourceInfo& other);
		SSingleResourceInfo operator+(const SSingleResourceInfo& other) const;
		SSingleResourceInfo operator-(const SSingleResourceInfo& other) const;

		//! 将单个资源信息转为PE信息头
		std::vector<SConvertedResData> Convert2ResData() const;
	};

	


#pragma endregion 中间转储



#pragma region 全局函数

	//! 通过窗口类名获取id
	WORD GetClassIdFromName(const CStringW& className);

	size_t AddStringToResource(std::vector<BYTE>& resource, size_t offset,
		const CStringW& str);

	CStringW GetClassNameFromRc(WORD wClassId);

    // 将 Win32 风格标志转换为可读文本（例如 "WS_CHILD|WS_VISIBLE"）。
    // 用于 UI 显示（树/属性），而非用于精确的序列化/反序列化（round-tripping）。
	CStringW FormatWindowStyleText(DWORD style);
	CStringW FormatWindowExStyleText(DWORD exStyle);

    // ---- 深拷贝辅助（打破 shared_ptr 别名引用） ----
	std::shared_ptr<SCtrlInfo> DeepCloneCtrlInfo(const std::shared_ptr<SCtrlInfo>& src);
	std::shared_ptr<SDialogInfo> DeepCloneDialogInfo(const std::shared_ptr<SDialogInfo>& src);
	std::shared_ptr<SMenuInfo> DeepCloneMenuInfo(const std::shared_ptr<SMenuInfo>& src);
	SSingleResourceInfo DeepCloneResourceInfo(const SSingleResourceInfo& src);



#pragma endregion 全局函数

}


/*!

	1、combox的交互存在问题
	1.1 combox的展开高度无法设置，展开后应当无法移动 -实现
	2、动态调整时的参考线只有左侧，且根据参考线放置的位置也并不是参考线的对应的位置 - 实现
	3、bage无法通过勾选树节点，获取当前combox的选项，并根据当前levelmask的设置绘制，要求修改后立即刷新窗口显示，如果树节点不勾选就不管 - 实现
	4、文件交互，需要删除ini的文件写入写出，并且不支持新建，只能读取pe文件，修改后保存/另存为 - 实现
	5、删除所有添加控件的功能，控件只能通过pe文件读取获得 -实现
	6、CDesignerSurfaceWnd绘制相关
		6.1	控件显示的文本比控件本身更长，需要调整一下对话框精度或者实时计算 - 实现
		6.2 checkbox被绘制成普通的pushbutton - 实现
		6.3 list，tree等控件无edge，导致不容易分辨边界 - 实现
		6.4 点击groupbox时，如果groupbox中无控件，则不容易选中gropubox - 实现
	7、CFormRcEditor显示相关
		7.1 属性表中如果使用_variant_t表示bool值时，在多字节字符集下的中文会乱码 - 实现
	8、CFormLevelManagerEditor 相关
		8.1 等级管理器中，不需要显示菜单的信息，只需要显示对话框和工具栏的信息 - 实现
	9、core相关
		9.1 带有文本的信息如RT_DIALOG,RT_STRING,RT_MENU,RT_ACCELERATOR等需要支持导出原生的rc格式的文本，RT_VERSION,RT_DLGINIT(不处理)
		9.2 字符串转换统一由 GovernLens/utils/TextConversion.h 提供
		9.3 SDialogInfo,SCtrlInfo等结构体需要添加拷贝构造函数，方便深拷贝
		9.4 是否需要将一些静态函数迁移到core中？？

	10、需要新增一个继承自CFormRcEdtor的CFormModifyRcEditor，用于修改已有的rc文件
		10.1 该类需要一个controler，用于添加互相比较PE资源的功能，并比对出新增结果，通过新增结果快速从CFormRcEdtor的树节点切换到对应窗口并选中新增的控件
		10.2 该类修改完成后需要生成一个RC文本，用于源代码处增加rc


*/





/* ===================== 原文件：RcEditorModel.h ===================== */
// [已合并依赖] #include "RcCore.h"

namespace rceditor
{
	class PEFileEditor
	{
	public:
		//! 指定需要读取的资源文件路径
		explicit PEFileEditor(const CString& csFilePath);
		~PEFileEditor();

	public:
		//! 获取资源信息
		SSingleResourceInfo& GetResourceInfo()
		{
			return m_oResourceInfo;
		}

		//! 获取资源文件名
		inline CString GetResourceFileName() const
		{
			return m_csFileName;
		}

		//! 回写资源信息（用于：UI编辑副本 -> 内部资源）
		//! 语义：真正深拷贝。不会与入参共享内部 shared_ptr 指向的对象。
		void SetResourceInfo(const SSingleResourceInfo& info)
		{
			m_oResourceInfo = DeepCloneResourceInfo(info);
		}

		enum EResourceError : unsigned int
		{
			ERR_SUCCESS = 0,
			ERR_FILE_NOT_FOUND = 1 << 0, // 0x0001
			ERR_INVALID_PE = 1 << 1, // 0x0002
			ERR_DIALOG_FAIL = 1 << 2, // 0x0004
			ERR_STRINGTABLE_FAIL = 1 << 3, // 0x0008
			ERR_MENU_FAIL = 1 << 4, // 0x0010
			ERR_VERSION_FAIL = 1 << 5, // 0x0020
			ERR_TOOLBAR_FAIL = 1 << 6  // 0x0040
		};
		//! 读取PE资源信息
		/*!
			\brief 读取指定文件路径的PE资源信息
			\return EResourceError
		*/
        EResourceError ReadPEInfo(LANGID targetLanguage = 0);

		//! 更新PE资源信息
		/*!
			\brief 比较后更新当前加载文件的PE资源信息
			\return 更新成功返回 true 否则返回 false
		*/
		bool UpDatePEInfo();

		//! 另存为：将当前资源写入到指定目标文件
		/*!
			\brief 将当前已解析/编辑的资源写入一个新的PE文件路径
			\param csTargetFilePath 目标exe/dll路径
			\return 成功返回 true
		*/
		bool UpDatePEInfoAs(const CString& csTargetFilePath);

	protected: //! 加载和卸载模块
		bool LoadModule();
		void UnLoadModule();

	protected:	//! 帮助函数
		CString GetFolderPathFromFilePath(const CString& csFilePath);
		//! 获取当前日期字符串 yy-MM-dd-HH-mm-ss
		CString GetCurDate();

		// 帮助函数
		WORD* SkipString(WORD* pWord);
		WORD* AlignDWORD(WORD* pWord);

	private:	//! 解析 PE文件 -- 获取资源ID
		//! 解析指定的资源类型
    BOOL EnumResourceTypesFromFilePath(HMODULE hModule, LPCTSTR lpszType, std::vector<SPerLanResID>* vtResIDs);

		//! 枚举资源中的名字
		/*!
			\brief 枚举资源中的名字
			\param hModule 模块句柄
			\param lpszType 资源类型
			\param lpszName 资源名字
			\param lParam 获取的资源ID存放的地址
		*/
    static BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);

		//! 枚举资源中的语言
		/*!
			\brief 枚举资源中的语言
			\param hModule 模块句柄
			\param lpszType 资源类型
			\param lpszName 资源名字
			\param wLang 资源语言ID
			\param lParam 获取的资源语言ID存放的地址
		*/
    static BOOL CALLBACK EnumResLangProc(HMODULE hModule, LPCTSTR lpszType, LPCTSTR lpszName, WORD wLang, LONG_PTR lParam);

	private://! 解析具体的资源
		//! 窗口
		//! 获取指定文件路径的每种语言的对话框信息
		/*!
			\param perLanResID 资源类型为dialog的每种语言的资源ID
		*/
		void GetPerlanDlgInfoFromFilePath(const std::vector<SPerLanResID>& vecPerLanID);

		//! stringtable
		/*!
			\param perLanResID 资源类型为stringtable的每种语言的资源ID
		*/
		void GetPerlanStringTableFromFilePath( const std::vector<SPerLanResID>&vecPerLanID);

		//! version信息
		/*!
			\param perLanResID 资源类型为version的每种语言的资源ID
		*/
		void GetPerlanVersionInfoFromFilePath( const std::vector<SPerLanResID>& vecPerLanID);

		//! 菜单信息
		/*!
			\param perLanResID 资源类型为menu的每种语言的资源ID
		*/
		void GetPerlanMenuInfoFromFilePath(const std::vector<SPerLanResID>& vecPerLanID);
		void ParseMenu(const BYTE* data, size_t size, std::vector<std::shared_ptr<SMenuInfo>>& outMenus);
		void ParseMenuSub(const BYTE*& p, const BYTE* end, std::vector<std::shared_ptr<SMenuInfo>>& vec);

		//! toolbar信息
		/*!
			\param perLanResID 资源类型为toolbar的每种语言的资源ID
		*/
		void GetPerlanToolbarInfoFromFilePath(const std::vector<SPerLanResID>& vecPerLanID);

	private:
		HMODULE m_hModule; /* 资源模块句柄 */
		CString m_csFilePath; /* 资源文件路径 */
		CString m_csFileName; /* 资源文件名 */
	private:
		SSingleResourceInfo m_oResourceInfo; /* 资源信息 */

	};
}




/* ===================== 原文件：DebugPageLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]

namespace DebugPageLevel
{
    enum PageId
    {
        Nozzle = 0xF010, Scan = 0xF011, Alignment = 0xF012,
        Parameters = 0xF002, Region = 0xF001, Test = 0xF015,
        Study = 0xF016, SpecialPad = 0xF017
    };
    inline int PageOrder(unsigned int id)
    {
        const unsigned int ids[] = {Nozzle, Scan, Alignment, Parameters, Region, Test, Study, SpecialPad};
        for (int i = 0; i < 8; ++i) if (ids[i] == id) return i;
        return -1;
    }
    inline unsigned int ToolbarPage(unsigned int id)
    {
        switch (id)
        {
        case 200: return Alignment;
        case 201: return Parameters;
        case 204: return Region;
        case 245: return Study;
        case 487: return SpecialPad;
        default: return 0;
        }
    }
    inline bool IsLegacyNavigation(unsigned int dialogId, unsigned int controlId)
    {
        return dialogId == Region && controlId >= 150 && controlId <= 157;
    }
}


/* ===================== 原文件：ToolHistogramLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]

namespace ToolHistogramLevel
{
    enum { ColorDialog = 134, StudyDialog = 3010, Histogram = 62710,
        FirstChannel = 62711, ChannelCount = 7 };

    inline unsigned int ChannelControlId(int channel)
    {
        return channel >= 0 && channel < ChannelCount ? FirstChannel + channel : 0;
    }
}


/*
==================================================================================
                                二、配置、本地化与审计策略
==================================================================================
*/

/* ===================== JSON 工具已迁移至 GovernLens/utils/JsonUtils.h ===================== */

/* ===================== 原文件：LevelLocalize.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#ifndef LEVEL_LOCALIZE
#define LEVEL_LOCALIZE
#include <afx.h>
// [已合并依赖] #include "RcCore.h"

// 在全局作用域重用 RcCore 中的 CrossPlatformStringForLevel 以保持兼容性
using rceditor::CrossPlatformStringForLevel;


// 全局当前语言索引（与 oldlevel 保持一致）
// 通过 using 声明允许旧代码直接使用未限定的 `gbCurNeedShowLan`，
// 同时保持 `rceditor` 命名空间中为权威符号。优先使用 using-declaration
// 而非宏以避免意外的标记替换（例如当代码已经使用限定名时可能产生
// `rceditor::rceditor::gbCurNeedShowLan` 之类的问题）。
using rceditor::gbCurNeedShowLan;

inline LANGID CurrentLevelResourceLanguage()
{
    const LANGID languages[] = {0,
        MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
        MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT),
        MAKELANGID(LANG_THAI, SUBLANG_DEFAULT),
        MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT),
        MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL)};
    return gbCurNeedShowLan >= 1 && gbCurNeedShowLan <= 7 ? languages[gbCurNeedShowLan] : 0;
}


// 以下宏风格与旧工程 CLevelManagerDefine.h 保持一致，便于扩展多语言字符串

#define LIST_HEADER_LEVEL_STATUS(lan) \
    (lan) == 1 ? u8"状态" : \
    (lan) == 2 ? u8"Status" : \
    (lan) == 3 ? u8"状態" : \
    (lan) == 4 ? u8"상태" : \
    (lan) == 5 ? u8"สถานะ" : \
    (lan) == 6 ? u8"Trạng thái" : \
    (lan) == 7 ? u8"狀態" : \
    u8"Status"

#define LIST_HEADER_LEVEL_CTRL_DESCRIPTHION(lan) \
    ((lan) == 1 ? u8"控件描述" : \
    (lan) == 2 ? u8"Control description" : \
    (lan) == 3 ? u8"コントロールの説明" : \
    (lan) == 4 ? u8"컨트롤 설명" : \
    (lan) == 5 ? u8"คำอธิบายของคอนโทรล" : \
    (lan) == 6 ? u8"Mô tả điều khiển" : \
    (lan) == 7 ? u8"控制說明" : \
    u8"Control description")

#define LIST_HEADER_LEVEL_CTRL_TYPE(lan) \
    ((lan) == 1 ? u8"控件类型" : \
    (lan) == 2 ? u8"Control type" : \
    (lan) == 3 ? u8"コントロールタイプ" : \
    (lan) == 4 ? u8"컨트롤 유형" : \
    (lan) == 5 ? u8"ประเภทการควบคุม" : \
    (lan) == 6 ? u8"Loại điều khiển" : \
    (lan) == 7 ? u8"控件類型" : \
    u8"Control type")

#define LIST_HEADER_LEVEL_CTRL_ID(lan) \
    ((lan) == 1 ? u8"控件ID" : \
    (lan) == 2 ? u8"Control ID" : \
    (lan) == 3 ? u8"コントロールID" : \
    (lan) == 4 ? u8"컨트롤 ID" : \
    (lan) == 5 ? u8"รหัสควบคุม" : \
    (lan) == 6 ? u8"ID điều khiển" : \
    (lan) == 7 ? u8"控件編號" : \
    u8"Control ID")

#define LIST_HEADER_LEVEL_CTRL_LEVEL(l) \
    ((l) == 1 ? u8"等级-%d" : \
    (l) == 2 ? u8"Grade -%d" : \
    (l) == 3 ? u8"レベル-%d" : \
    (l) == 4 ? u8"레벨-%d" : \
    (l) == 5 ? u8"ระดับ - %d" : \
    (l) == 6 ? u8"Cấp độ -%d" : \
    (l) == 7 ? u8"等級-%d" : \
    u8"Grade -%d")

#define LIST_HEADER_LEVEL_CTRL_ALLOW_EDIT(lan) \
    ((lan) == 1 ? u8"允许编辑" : \
    (lan) == 2 ? u8"Allow edit" : \
    (lan) == 3 ? u8"編集を許可する" : \
    (lan) == 4 ? u8"편집 허용" : \
    (lan) == 5 ? u8"อนุญาตให้แก้ไข" : \
    (lan) == 6 ? u8"Cho phép chỉnh sửa" : \
    (lan) == 7 ? u8"允許編輯" : \
    u8"Allow edit")

#define LIST_HEADER_LEVEL_CTRL_DISALLOW_EDIT(lan) \
    ((lan) == 1 ? u8"禁止编辑" : \
    (lan) == 2 ? u8"Disallow edit" : \
    (lan) == 3 ? u8"編集を禁止する" : \
    (lan) == 4 ? u8"편집 금지" : \
    (lan) == 5 ? u8"ไม่อนุญาตให้แก้ไข" : \
    (lan) == 6 ? u8"Không cho phép chỉnh sửa" : \
    (lan) == 7 ? u8"禁止編輯" : \
    u8"Disallow edit")

// 等级树业务分类，显示语言沿用等级管理器的 1..7 编号。
#define LEVEL_GROUP_DEBUG_PAGES(lan) \
    ((lan) == 1 ? u8"调试页面" : \
    (lan) == 2 ? u8"Debug pages" : \
    (lan) == 3 ? u8"デバッグページ" : \
    (lan) == 4 ? u8"디버그 페이지" : \
    (lan) == 5 ? u8"หน้าดีบัก" : \
    (lan) == 6 ? u8"Trang gỡ lỗi" : \
    (lan) == 7 ? u8"調試頁面" : \
    u8"Debug pages")

#define LEVEL_GROUP_DOCUMENT(lan) \
    ((lan) == 1 ? u8"文档设定" : \
    (lan) == 2 ? u8"Document settings" : \
    (lan) == 3 ? u8"ドキュメント設定" : \
    (lan) == 4 ? u8"문서 설정" : \
    (lan) == 5 ? u8"การตั้งค่าเอกสาร" : \
    (lan) == 6 ? u8"Thiết lập tài liệu" : \
    (lan) == 7 ? u8"文檔設定" : \
    u8"Document settings")

#define LEVEL_GROUP_ADVANCED(lan) \
    ((lan) == 1 ? u8"高级设定" : \
    (lan) == 2 ? u8"Advanced settings" : \
    (lan) == 3 ? u8"詳細設定" : \
    (lan) == 4 ? u8"고급 설정" : \
    (lan) == 5 ? u8"การตั้งค่าขั้นสูง" : \
    (lan) == 6 ? u8"Thiết lập nâng cao" : \
    (lan) == 7 ? u8"進階設定" : \
    u8"Advanced settings")

#define LEVEL_GROUP_LIGHT(lan) \
    ((lan) == 1 ? u8"光源设定" : \
    (lan) == 2 ? u8"Light settings" : \
    (lan) == 3 ? u8"照明設定" : \
    (lan) == 4 ? u8"조명 설정" : \
    (lan) == 5 ? u8"การตั้งค่าแสง" : \
    (lan) == 6 ? u8"Thiết lập nguồn sáng" : \
    (lan) == 7 ? u8"光源設定" : \
    u8"Light settings")

#define LEVEL_GROUP_COLOR(lan) \
    ((lan) == 1 ? u8"颜色设定" : \
    (lan) == 2 ? u8"Color settings" : \
    (lan) == 3 ? u8"色設定" : \
    (lan) == 4 ? u8"색상 설정" : \
    (lan) == 5 ? u8"การตั้งค่าสี" : \
    (lan) == 6 ? u8"Thiết lập màu sắc" : \
    (lan) == 7 ? u8"顏色設定" : \
    u8"Color settings")

// 仅装饰等级树标签，不修改资源标题或 JSON 中的窗口名称。
#define LEVEL_DOC_BLIND_CONDITION(lan) \
    ((lan) == 1 ? u8"（条件页：启用盲孔）" : \
    (lan) == 2 ? u8" (Conditional: blind via enabled)" : \
    (lan) == 3 ? u8"（条件付き：ブラインドビア有効時）" : \
    (lan) == 4 ? u8"（조건부: 블라인드 비아 활성화）" : \
    (lan) == 5 ? u8"（ตามเงื่อนไข: เปิดใช้รูบอด）" : \
    (lan) == 6 ? u8"（Có điều kiện: bật lỗ mù）" : \
    (lan) == 7 ? u8"（條件頁：啟用盲孔）" : \
    u8" (Conditional: blind via enabled)")

#define LEVEL_DOC_HOLE_CONDITION(lan) \
    ((lan) == 1 ? u8"（条件页：启用验孔相机）" : \
    (lan) == 2 ? u8" (Conditional: hole inspection camera enabled)" : \
    (lan) == 3 ? u8"（条件付き：穴検査カメラ有効時）" : \
    (lan) == 4 ? u8"（조건부: 홀 검사 카메라 활성화）" : \
    (lan) == 5 ? u8"（ตามเงื่อนไข: เปิดใช้กล้องตรวจรู）" : \
    (lan) == 6 ? u8"（Có điều kiện: bật camera kiểm tra lỗ）" : \
    (lan) == 7 ? u8"（條件頁：啟用驗孔相機）" : \
    u8" (Conditional: hole inspection camera enabled)")

// 控件显示
#define LEVEL_CTRL_DESCRIPTION_CONFIRM(lan) \
    ((lan) == 1 ? u8"确定" : \
    (lan) == 2 ? u8"OK" : \
    (lan) == 3 ? u8"確定です" : \
    (lan) == 4 ? u8"확인" : \
    (lan) == 5 ? u8"ตกลง" : \
    (lan) == 6 ? u8"Chọn" : \
    (lan) == 7 ? u8"確定" : \
    u8"OK")

#define LEVEL_CTRL_DESCRIPTION_APPLY(lan) \
    ((lan) == 1 ? u8"应用" : \
    (lan) == 2 ? u8"Apply" : \
    (lan) == 3 ? u8"適用" : \
    (lan) == 4 ? u8"적용" : \
    (lan) == 5 ? u8"นำไปใช้" : \
    (lan) == 6 ? u8"Áp dụng" : \
    (lan) == 7 ? u8"應用" : \
    u8"Apply")

#define LEVEL_CTRL_DESCRIPTION_CANCEL(lan) \
    ((lan) == 1 ? u8"取消" : \
    (lan) == 2 ? u8"Cancel" : \
    (lan) == 3 ? u8"キャンセル" : \
    (lan) == 4 ? u8"취소" : \
    (lan) == 5 ? u8"ยกเลิก" : \
    (lan) == 6 ? u8"Hủy" : \
    (lan) == 7 ? u8"取消" : \
    u8"Cancel")

#define LEVEL_CTRL_DESCRIPTION_SEARCH(lan) \
    ((lan) == 1 ? u8"搜索" : \
    (lan) == 2 ? u8"Search" : \
    (lan) == 3 ? u8"検索" : \
    (lan) == 4 ? u8"검색" : \
    (lan) == 5 ? u8"ค้นหา" : \
    (lan) == 6 ? u8"Tìm kiếm" : \
    (lan) == 7 ? u8"搜尋" : \
    u8"Search")

#define LEVEL_CTRL_DESCRIPTION_CLEAR(lan) \
    ((lan) == 1 ? u8"清除" : \
    (lan) == 2 ? u8"Clear" : \
    (lan) == 3 ? u8"クリア" : \
    (lan) == 4 ? u8"지우기" : \
    (lan) == 5 ? u8"เคลียร์" : \
    (lan) == 6 ? u8"Xóa" : \
    (lan) == 7 ? u8"清除" : \
    u8"Clear")

#define LEVEL_CTRL_DESCRIPTION_SELECT(lan) \
    ((lan) == 1 ? u8"全选/反选" : \
    (lan) == 2 ? u8"Select all/Reverse select" : \
    (lan) == 3 ? u8"全選/逆選" : \
    (lan) == 4 ? u8"전체 선택/역선택" : \
    (lan) == 5 ? u8"เลือกทั้งหมด/ยกเลิกเลือก" : \
    (lan) == 6 ? u8"Chọn tất cả/Đảo chọn" : \
    (lan) == 7 ? u8"全選/反選" : \
    u8"Select all/Reverse select")

// Property grid header (left = name, right = value)
#define PROPERTY_GRID_HEADER_LEFT(lan) \
    ((lan) == 1 ? u8"属性" : \
    (lan) == 2 ? u8"Property" : \
    (lan) == 3 ? u8"プロパティ" : \
    (lan) == 4 ? u8"속성" : \
    (lan) == 5 ? u8"คุณสมบัติ" : \
    (lan) == 6 ? u8"Thuộc tính" : \
    (lan) == 7 ? u8"屬性" : \
    u8"Property")

#define PROPERTY_GRID_HEADER_RIGHT(lan) \
    ((lan) == 1 ? u8"值" : \
    (lan) == 2 ? u8"Value" : \
    (lan) == 3 ? u8"値" : \
    (lan) == 4 ? u8"값" : \
    (lan) == 5 ? u8"ค่า" : \
    (lan) == 6 ? u8"Giá trị" : \
    (lan) == 7 ? u8"值" : \
    u8"Value")

// Additional common UI strings used by editors
#define UI_OPEN(l) ((l) == 1 ? u8"加载资源EXE或DLL" : \
    (l) == 2 ? u8"Load Resource EXE or DLL" : \
    (l) == 3 ? u8"リソースEXEまたはDLLを読み込む" : \
    (l) == 4 ? u8"리소스 EXE 또는 DLL 로드" : \
    (l) == 5 ? u8"โหลดทรัพยากร EXE หรือ DLL" : \
    (l) == 6 ? u8"Tải tài nguyên EXE hoặc DLL" : \
    (l) == 7 ? u8"加載資源EXE或DLL" : \
    u8"Load Resource EXE or DLL")

#define UI_SAVE(l) ((l) == 1 ? u8"保存" : \
    (l) == 2 ? u8"Save" : \
    (l) == 3 ? u8"保存" : \
    (l) == 4 ? u8"저장" : \
    (l) == 5 ? u8"บันทึก" : \
    (l) == 6 ? u8"Lưu" : \
    (l) == 7 ? u8"儲存" : \
    u8"Save")

#define UI_SAVE_AS(l) ((l) == 1 ? u8"导出副本" : \
    (l) == 2 ? u8"Export Copy" : \
    (l) == 3 ? u8"コピーをエクスポート" : \
    (l) == 4 ? u8"사본 내보내기" : \
    (l) == 5 ? u8"ส่งออกสำเนา" : \
    (l) == 6 ? u8"Xuất bản sao" : \
    (l) == 7 ? u8"導出副本" : \
    u8"Export Copy")

#define UI_SEARCH(l) ((l) == 1 ? u8"搜索" : \
    (l) == 2 ? u8"Search" : \
    (l) == 3 ? u8"検索" : \
    (l) == 4 ? u8"검색" : \
    (l) == 5 ? u8"ค้นหา" : \
    (l) == 6 ? u8"Tìm kiếm" : \
    (l) == 7 ? u8"搜尋" : \
    u8"Search")

#define UI_CLEAR(l) ((l) == 1 ? u8"清除" : \
    (l) == 2 ? u8"Clear" : \
    (l) == 3 ? u8"クリア" : \
    (l) == 4 ? u8"지우기" : \
    (l) == 5 ? u8"เคลียร์" : \
    (l) == 6 ? u8"Xóa" : \
    (l) == 7 ? u8"清除" : \
    u8"Clear")

#define UI_SELECT_ALL(l) ((l) == 1 ? u8"全选" : \
    (l) == 2 ? u8"Select all" : \
    (l) == 3 ? u8"すべて選択" : \
    (l) == 4 ? u8"모두 선택" : \
    (l) == 5 ? u8"เลือกทั้งหมด" : \
    (l) == 6 ? u8"Chọn tất cả" : \
    (l) == 7 ? u8"全選" : \
    u8"Select all")

#define UI_FILE_MENU(l) ((l) == 1 ? u8"文件(&F)" : \
    (l) == 2 ? u8"File(&F)" : \
    (l) == 3 ? u8"ファイル(&F)" : \
    (l) == 4 ? u8"파일(&F)" : \
    (l) == 5 ? u8"ไฟล์(&F)" : \
    (l) == 6 ? u8"Tệp(&F)" : \
    (l) == 7 ? u8"檔案(&F)" : \
    u8"File(&F)")

#define UI_EXIT(l) ((l) == 1 ? u8"退出(&X)" : \
    (l) == 2 ? u8"Exit(&X)" : \
    (l) == 3 ? u8"終了(&X)" : \
    (l) == 4 ? u8"종료(&X)" : \
    (l) == 5 ? u8"ออก(&X)" : \
    (l) == 6 ? u8"Thoát(&X)" : \
    (l) == 7 ? u8"離開(&X)" : \
    u8"Exit(&X)")

#pragma region JSON 相关
// JSON 相关
#define UI_OPEN_JSON_MENU(l) ((l) == 1 ? u8"打开等级JSON..." : \
    (l) == 2 ? u8"Open Level JSON..." : \
    (l) == 3 ? u8"レベルJSONを開く..." : \
    (l) == 4 ? u8"레벨 JSON 열기..." : \
    (l) == 5 ? u8"เปิด Level JSON..." : \
    (l) == 6 ? u8"Mở Level JSON..." : \
    (l) == 7 ? u8"打開等級JSON..." : \
    u8"Open Level JSON...")

#define UI_SAVE_JSON_MENU(l) ((l) == 1 ? u8"另存为等级JSON..." : \
(l) == 2 ? u8"Save Level JSON..." : \
(l) == 3 ? u8"レベルJSONを保存..." : \
(l) == 4 ? u8"레벨 JSON으로 저장..." : \
(l) == 5 ? u8"บันทึก Level JSON..." : \
(l) == 6 ? u8"Lưu Level JSON..." : \
(l) == 7 ? u8"另存為等級JSON..." : \
u8"Save Level JSON...")

// Messages
#define UI_MSG_LEVEL_JSON_MODIFIED_PROMPT(l) ((l) == 1 ? u8"等级配置已修改，是否保存到 JSON？" : \
(l) == 2 ? u8"Level configuration modified, save to JSON?" : \
(l) == 3 ? u8"レベル設定が変更されました。JSONに保存しますか？" : \
(l) == 4 ? u8"레벨 구성이 변경되었습니다. JSON으로 저장하시겠습니까?" : \
(l) == 5 ? u8"การตั้งค่าระดับถูกแก้ไข ต้องการบันทึกเป็น JSON หรือไม่?" : \
(l) == 6 ? u8"Cấu hình cấp độ đã thay đổi, có lưu vào JSON không?" : \
(l) == 7 ? u8"等級配置已修改，是否儲存為 JSON？" : \
u8"Level configuration modified, save to JSON?")

#define UI_MSG_JSON_LOADED(l) ((l) == 1 ? u8"已加载等级配置 JSON。" : \
(l) == 2 ? u8"Level configuration JSON loaded." : \
(l) == 3 ? u8"レベル設定のJSONを読み込みました。" : \
(l) == 4 ? u8"레벨 구성 JSON을 불러왔습니다." : \
(l) == 5 ? u8"โหลด JSON การกำหนดระดับเรียบร้อยแล้ว" : \
(l) == 6 ? u8"Đã tải JSON cấu hình cấp độ." : \
(l) == 7 ? u8"已載入等級配置 JSON。" : \
u8"Level configuration JSON loaded.")

#define UI_MSG_JSON_LOAD_FAILED(l) ((l) == 1 ? u8"加载 JSON 失败。" : \
(l) == 2 ? u8"Failed to load JSON." : \
(l) == 3 ? u8"JSONの読み込みに失敗しました。" : \
(l) == 4 ? u8"JSON을 불러오지 못했습니다." : \
(l) == 5 ? u8"โหลด JSON ล้มเหลว" : \
(l) == 6 ? u8"Tải JSON thất bại." : \
(l) == 7 ? u8"載入 JSON 失敗。" : \
u8"Failed to load JSON.")

#define UI_MSG_JSON_SAVED(l) ((l) == 1 ? u8"已保存等级配置 JSON。" : \
(l) == 2 ? u8"Level configuration JSON saved." : \
(l) == 3 ? u8"レベル設定のJSONを保存しました。" : \
(l) == 4 ? u8"레벨 구성 JSON을 저장했습니다." : \
(l) == 5 ? u8"บันทึก JSON การกำหนดระดับเรียบร้อยแล้ว" : \
(l) == 6 ? u8"Đã lưu JSON cấu hình cấp độ." : \
(l) == 7 ? u8"已儲存等級配置 JSON。" : \
u8"Level configuration JSON saved.")

#define UI_MSG_JSON_SAVE_FAILED(l) ((l) == 1 ? u8"保存 JSON 失败。" : \
(l) == 2 ? u8"Failed to save JSON." : \
(l) == 3 ? u8"JSONの保存に失敗しました。" : \
(l) == 4 ? u8"JSON 저장에 실패했습니다." : \
(l) == 5 ? u8"บันทึก JSON ล้มเหลว" : \
(l) == 6 ? u8"Lưu JSON thất bại." : \
(l) == 7 ? u8"儲存 JSON 失敗。" : \
u8"Failed to save JSON.")

// File dialog filter for JSON files (localized)
#define UI_JSON_FILE_FILTER(l) ((l) == 1 ? u8"JSON (*.json)|*.json|所有文件 (*.*)|*.*||" : \
    (l) == 2 ? u8"JSON (*.json)|*.json|All files (*.*)|*.*||" : \
    (l) == 3 ? u8"JSON (*.json)|*.json|すべてのファイル (*.*)|*.*||" : \
    (l) == 4 ? u8"JSON (*.json)|*.json|모든 파일 (*.*)|*.*||" : \
    (l) == 5 ? u8"JSON (*.json)|*.json|ไฟล์ทั้งหมด (*.*)|*.*||" : \
    (l) == 6 ? u8"JSON (*.json)|*.json|Tất cả các tệp (*.*)|*.*||" : \
    (l) == 7 ? u8"JSON (*.json)|*.json|所有檔案 (*.*)|*.*||" : \
    u8"JSON (*.json)|*.json|All files (*.*)|*.*||")

// 内部标记：用于 JSON 文件中表示嵌入资源的占位符
#define LM_BINNAME_EMBEDDED u8"<embedded>"

// 其他消息
#define UI_MSG_UNKNOWN_ERROR(l) \
    ((l) == 1 ? u8"未知错误，请重启程序" : \
    (l) == 2 ? u8"Unknown error, please restart the application" : \
    (l) == 3 ? u8"不明なエラーが発生しました。アプリケーションを再起動してください" : \
    (l) == 4 ? u8"알 수 없는 오류가 발생했습니다. 프로그램을 재시작하세요" : \
    (l) == 5 ? u8"เกิดข้อผิดพลาดที่ไม่ทราบสาเหตุ กรุณารีสตาร์ทโปรแกรม" : \
    (l) == 6 ? u8"Lỗi không xác định, vui lòng khởi động lại ứng dụng" : \
    (l) == 7 ? u8"未知錯誤，請重新啟動程式" : \
    u8"Unknown error, please restart the application")

#define UI_MSG_SYSTEM_UNICODE_WARNING(l) \
((l) == 1 ? u8"检测到系统为 Unicode 环境，但资源配置中的对话框运行时类名为空。\n请更换或更新等级管理配置文件以包含运行时类名（csClassPath 或 csClassName）。" : \
(l) == 2 ? u8"System appears to be Unicode-enabled but dialog runtime class names are missing from configuration. Please update the level manager JSON to include csClassPath or csClassName." : \
(l) == 3 ? u8"システムはUnicode環境のようですが、設定に対話の実行時クラス名がありません。csClassPathまたはcsClassNameを含むように設定ファイルを更新してください。" : \
(l) == 4 ? u8"시스템이 유니코드 환경으로 감지되었지만 설정에 대화 상자의 런타임 클래스 이름이 없습니다. csClassPath 또는 csClassName을 포함하도록 구성 파일을 업데이트하세요." : \
(l) == 5 ? u8"ตรวจพบว่าระบบเป็นสภาพแวดล้อม Unicode แต่ชื่อคลาสรันไทม์ของไดอะล็อกหายไปในการกำหนดค่า โปรดอัปเดตไฟล์ JSON ของ Level Manager ให้รวม csClassPath หรือ csClassName" : \
(l) == 6 ? u8"Hệ thống phát hiện hỗ trợ Unicode nhưng tên lớp runtime của hộp thoại thiếu trong cấu hình. Vui lòng cập nhật JSON của Level Manager để bao gồm csClassPath hoặc csClassName." : \
(l) == 7 ? u8"檢測到系統為 Unicode 環境，但資源配置中的對話框運行時類名為空。\n請更換或更新等級管理配置文件以包含運行時類名（csClassPath 或 csClassName）。" : \
u8"System appears to be Unicode-enabled but dialog runtime class names are missing from configuration. Please update the level manager JSON to include csClassPath or csClassName.")

#define UI_WINDOW_TITLE_LEVEL_MANAGER_PREFIX(l) \
    ((l) == 1 ? u8"等级管理器 - " : \
    (l) == 2 ? u8"LevelManager - " : \
    (l) == 3 ? u8"レベル マネージャ - " : \
    (l) == 4 ? u8"레벨 관리자 - " : \
    (l) == 5 ? u8"ตัวจัดการเลเวล - " : \
    (l) == 6 ? u8"Trình quản lý cấp độ - " : \
    (l) == 7 ? u8"等級 管理器 - " : \
    u8"Level Manager - ")

// Property grid labels and common group names
#define UI_INFO(l) ((l) == 1 ? u8"信息" : \
    (l) == 2 ? u8"Info" : \
    (l) == 3 ? u8"情報" : \
    (l) == 4 ? u8"정보" : \
    (l) == 5 ? u8"ข้อมูล" : \
    (l) == 6 ? u8"Thông tin" : \
    (l) == 7 ? u8"資訊" : \
    u8"Info")

#define UI_SELECTION(l) ((l) == 1 ? u8"选择" : \
    (l) == 2 ? u8"Selection" : \
    (l) == 3 ? u8"選択" : \
    (l) == 4 ? u8"선택" : \
    (l) == 5 ? u8"การเลือก" : \
    (l) == 6 ? u8"Lựa chọn" : \
    (l) == 7 ? u8"選擇" : \
    u8"Selection")

#define UI_DIALOG(l) ((l) == 1 ? u8"对话框" : \
    (l) == 2 ? u8"Dialog" : \
    (l) == 3 ? u8"ダイアログ" : \
    (l) == 4 ? u8"대화상자" : \
    (l) == 5 ? u8"กล่องโต้ตอบ" : \
    (l) == 6 ? u8"Hộp thoại" : \
    (l) == 7 ? u8"對話框" : \
    u8"Dialog")

#define UI_CONTROL(l) ((l) == 1 ? u8"控件" : \
    (l) == 2 ? u8"Control" : \
    (l) == 3 ? u8"コントロール" : \
    (l) == 4 ? u8"컨트롤" : \
    (l) == 5 ? u8"ตัวควบคุม" : \
    (l) == 6 ? u8"Điều khiển" : \
    (l) == 7 ? u8"控制" : \
    u8"Control")

#define UI_TOOLBAR_INFO(l) ((l) == 1 ? u8"工具栏 信息" : \
    (l) == 2 ? u8"Toolbar Info" : \
    (l) == 3 ? u8"ツールバー情報" : \
    (l) == 4 ? u8"도구 모음 정보" : \
    (l) == 5 ? u8"ข้อมูลแถบเครื่องมือ" : \
    (l) == 6 ? u8"Thông tin thanh công cụ" : \
    (l) == 7 ? u8"工具欄 信息" : \
    u8"Toolbar Info")

#define UI_ID(l) ((l) == 1 ? u8"ID" : \
    (l) == 2 ? u8"ID" : \
    (l) == 3 ? u8"ID" : \
    (l) == 4 ? u8"ID" : \
    (l) == 5 ? u8"ID" : \
    (l) == 6 ? u8"ID" : \
    (l) == 7 ? u8"ID" : \
    u8"ID")

#define UI_CLASS(l) ((l) == 1 ? u8"类" : \
    (l) == 2 ? u8"Class" : \
    (l) == 3 ? u8"クラス" : \
    (l) == 4 ? u8"클래스" : \
    (l) == 5 ? u8"คลาส" : \
    (l) == 6 ? u8"Lớp" : \
    (l) == 7 ? u8"類" : \
    u8"Class")

#define UI_TEXT(l) ((l) == 1 ? u8"文本" : \
    (l) == 2 ? u8"Text" : \
    (l) == 3 ? u8"テキスト" : \
    (l) == 4 ? u8"텍스트" : \
    (l) == 5 ? u8"ข้อความ" : \
    (l) == 6 ? u8"Văn bản" : \
    (l) == 7 ? u8"文本" : \
    u8"Text")

#define UI_RECT_DLU(l) ((l) == 1 ? u8"矩形 (DLU)" : \
    (l) == 2 ? u8"Rect (DLU)" : \
    (l) == 3 ? u8"矩形 (DLU)" : \
    (l) == 4 ? u8"사각형 (DLU)" : \
    (l) == 5 ? u8"สี่เหลี่ยม (DLU)" : \
    (l) == 6 ? u8"Hình chữ nhật (DLU)" : \
    (l) == 7 ? u8"矩形 (DLU)" : \
    u8"Rect (DLU)")

#define UI_STYLE_TEXT(l) ((l) == 1 ? u8"风格 文本" : \
    (l) == 2 ? u8"Style Text" : \
    (l) == 3 ? u8"スタイル テキスト" : \
    (l) == 4 ? u8"스타일 텍스트" : \
    (l) == 5 ? u8"สไตล์ ข้อความ" : \
    (l) == 6 ? u8"Văn bản kiểu" : \
    (l) == 7 ? u8"风格 文本" : \
    u8"Style Text")

#define UI_EXSTYLE_TEXT(l) ((l) == 1 ? u8"扩展风格 文本" : \
    (l) == 2 ? u8"ExStyle Text" : \
    (l) == 3 ? u8"拡張スタイル テキスト" : \
    (l) == 4 ? u8"확장 스타일 텍스트" : \
    (l) == 5 ? u8"สไตล์ขยาย ข้อความ" : \
    (l) == 6 ? u8"Văn bản ExStyle" : \
    (l) == 7 ? u8"扩展风格 文本" : \
    u8"ExStyle Text")

#define UI_NON_RESOURCE(l) ((l) == 1 ? u8"非资源" : \
    (l) == 2 ? u8"Non-Resource" : \
    (l) == 3 ? u8"非リソース" : \
    (l) == 4 ? u8"비리소스" : \
    (l) == 5 ? u8"ไม่ใช่ทรัพยากร" : \
    (l) == 6 ? u8"Không phải tài nguyên" : \
    (l) == 7 ? u8"非资源" : \
    u8"Non-Resource")

#define UI_IN_LEVEL_MANAGER(l) ((l) == 1 ? u8"是否参与等级管理" : \
    (l) == 2 ? u8"In Level Manager" : \
    (l) == 3 ? u8"レベルマネージャに含めるか" : \
    (l) == 4 ? u8"레벨 관리자에 포함" : \
    (l) == 5 ? u8"รวมในตัวจัดการเลเวลไหม" : \
    (l) == 6 ? u8"Tham gia vào Level Manager" : \
    (l) == 7 ? u8"是否参与等级管理" : \
    u8"In Level Manager")

#define UI_STYLE_FLAGS(l) ((l) == 1 ? u8"风格 标志" : \
    (l) == 2 ? u8"Style Flags" : \
    (l) == 3 ? u8"スタイル フラグ" : \
    (l) == 4 ? u8"스타일 플래그" : \
    (l) == 5 ? u8"แฟลกสไตล์" : \
    (l) == 6 ? u8"Cờ kiểu" : \
    (l) == 7 ? u8"风格 标志" : \
    u8"Style Flags")

#define UI_EXSTYLE_FLAGS(l) ((l) == 1 ? u8"扩展风格 标志" : \
    (l) == 2 ? u8"ExStyle Flags" : \
    (l) == 3 ? u8"拡張スタイル フラグ" : \
    (l) == 4 ? u8"확장 스타일 플래그" : \
    (l) == 5 ? u8"แฟลกสไตล์ขยาย" : \
    (l) == 6 ? u8"Cờ ExStyle" : \
    (l) == 7 ? u8"扩展风格 标志" : \
    u8"ExStyle Flags")

#define UI_MULTILINE(l) ((l) == 1 ? u8"多行" : \
    (l) == 2 ? u8"Multiline" : \
    (l) == 3 ? u8"複数行" : \
    (l) == 4 ? u8"여러 줄" : \
    (l) == 5 ? u8"หลายบรรทัด" : \
    (l) == 6 ? u8"Nhiều dòng" : \
    (l) == 7 ? u8"多行" : \
    u8"Multiline")

#define UI_CURRENT_LEVEL_DISABLED(l) ((l) == 1 ? u8"当前等级是否禁用" : \
    (l) == 2 ? u8"Disabled at current level" : \
    (l) == 3 ? u8"現在のレベルで無効" : \
    (l) == 4 ? u8"현재 레벨에서 비활성화" : \
    (l) == 5 ? u8"ปิดการใช้งานที่ระดับปัจจุบัน" : \
    (l) == 6 ? u8"Vô hiệu ở cấp hiện tại" : \
    (l) == 7 ? u8"当前等级是否禁用" : \
    u8"Disabled at current level")

#define UI_LEVEL_LABEL_FMT(l) ((l) == 1 ? u8"等级 %d" : \
    (l) == 2 ? u8"Level %d" : \
    (l) == 3 ? u8"レベル %d" : \
    (l) == 4 ? u8"레벨 %d" : \
    (l) == 5 ? u8"ระดับ %d" : \
    (l) == 6 ? u8"Cấp độ %d" : \
    (l) == 7 ? u8"等级 %d" : \
    u8"Level %d")

// Category labels
#define UI_DIALOGS(l) ((l) == 1 ? u8"对话框" : \
    (l) == 2 ? u8"Dialogs" : \
    (l) == 3 ? u8"ダイアログ" : \
    (l) == 4 ? u8"대화상자" : \
    (l) == 5 ? u8"กล่องโต้ตอบ" : \
    (l) == 6 ? u8"Hộp thoại" : \
    (l) == 7 ? u8"对话框" : \
    u8"Dialogs")

#define UI_MENUS(l) ((l) == 1 ? u8"菜单" : \
    (l) == 2 ? u8"Menus" : \
    (l) == 3 ? u8"メニュー" : \
    (l) == 4 ? u8"메뉴" : \
    (l) == 5 ? u8"เมนู" : \
    (l) == 6 ? u8"Menu" : \
    (l) == 7 ? u8"菜单" : \
    u8"Menus")

#define UI_TOOLBARS(l) ((l) == 1 ? u8"工具栏" : \
    (l) == 2 ? u8"Toolbars" : \
    (l) == 3 ? u8"ツールバー" : \
    (l) == 4 ? u8"도구 모음" : \
    (l) == 5 ? u8"แถบเครื่องมือ" : \
    (l) == 6 ? u8"Thanh công cụ" : \
    (l) == 7 ? u8"工具栏" : \
    u8"Toolbars")

// Placeholder for empty file list
#define UI_NO_FILE(l) ((l) == 1 ? u8"<无文件>" : \
    (l) == 2 ? u8"<no file>" : \
    (l) == 3 ? u8"<ファイルなし>" : \
    (l) == 4 ? u8"<파일 없음>" : \
    (l) == 5 ? u8"<ไม่มีไฟล์>" : \
    (l) == 6 ? u8"<No file>" : \
    (l) == 7 ? u8"<無文件>" : \
    u8"<no file>")

// Common messages for FormRcEditor
#define UI_MSG_LEVEL_LANGUAGE_INCOMPLETE(l) ((l) == 1 ? u8"一部分等级说明缺少目标语言资源，请检查语言文件。" : \
    (l) == 2 ? u8"Some level descriptions lack resources for the selected language. Please check the language files." : \
    (l) == 3 ? u8"一部のレベル説明に選択言語のリソースがありません。言語ファイルを確認してください。" : \
    (l) == 4 ? u8"일부 등급 설명에 선택한 언어의 리소스가 없습니다. 언어 파일을 확인하세요." : \
    (l) == 5 ? u8"คำอธิบายระดับบางรายการไม่มีทรัพยากรภาษาที่เลือก โปรดตรวจสอบไฟล์ภาษา" : \
    (l) == 6 ? u8"Một số mô tả cấp độ thiếu tài nguyên cho ngôn ngữ đã chọn. Vui lòng kiểm tra tệp ngôn ngữ." : \
    (l) == 7 ? u8"部分等級說明缺少目標語言資源，請檢查語言檔案。" : \
    u8"Some level descriptions lack resources for the selected language. Please check the language files.")

#define UI_MSG_OPEN_PARSE_FAILED(l) ((l) == 1 ? u8"打开失败（解析资源错误）。" : \
    (l) == 2 ? u8"Open failed (resource parse error)." : \
    (l) == 3 ? u8"解析エラーで開けません。" : \
    (l) == 4 ? u8"열기 실패(리소스 구문 분석 오류)." : \
    (l) == 5 ? u8"เปิดล้มเหลว (วิเคราะห์ทรัพยากรล้มเหลว)." : \
    (l) == 6 ? u8"Mở thất bại (lỗi phân tích tài nguyên)." : \
    (l) == 7 ? u8"開啟失敗（資源解析錯誤）。" : \
    u8"Open failed (resource parse error).")

#define UI_MSG_PLEASE_OPEN_EXECUTABLE(l) ((l) == 1 ? u8"请先打开一个可执行文件/动态库。" : \
    (l) == 2 ? u8"Please open an executable or DLL first." : \
    (l) == 3 ? u8"先に実行ファイル/DLLを開いてください。" : \
    (l) == 4 ? u8"먼저 실행 파일/ DLL을 열어주세요." : \
    (l) == 5 ? u8"โปรดเปิดไฟล์ปฏิบัติการหรือ DLL ก่อน" : \
    (l) == 6 ? u8"Vui lòng mở tệp thực thi hoặc DLL trước." : \
    (l) == 7 ? u8"請先開啟執行檔或動態連結程式庫。" : \
    u8"Please open an executable or DLL first.")

#define UI_MSG_NO_RESOURCE_TO_SAVE(l) ((l) == 1 ? u8"当前没有可保存的资源数据。" : \
    (l) == 2 ? u8"There is no resource data to save." : \
    (l) == 3 ? u8"保存するリソースがありません。" : \
    (l) == 4 ? u8"저장할 리소스 데이터가 없습니다." : \
    (l) == 5 ? u8"ไม่มีข้อมูลทรัพยากรที่จะบันทึก" : \
    (l) == 6 ? u8"Không có dữ liệu tài nguyên để lưu." : \
    (l) == 7 ? u8"目前沒有可儲存的資源資料。" : \
    u8"There is no resource data to save.")

#define UI_MSG_INVALID_PATH_USE_SAVEAS(l) ((l) == 1 ? u8"当前文件路径无效，请使用“另存为”。" : \
    (l) == 2 ? u8"Current file path is invalid, please use Save As." : \
    (l) == 3 ? u8"現在のファイルパスが無効です。「名前を付けて保存」を使用してください。" : \
    (l) == 4 ? u8"현재 파일 경로가 유효하지 않습니다. 다른 이름으로 저장을 사용하세요." : \
    (l) == 5 ? u8"เส้นทางไฟล์ปัจจุบันไม่ถูกต้อง โปรดใช้ บันทึกเป็น" : \
    (l) == 6 ? u8"Đường dẫn tệp hiện tại không hợp lệ, vui lòng sử dụng Lưu dưới dạng" : \
    (l) == 7 ? u8"目前檔案路徑無效，請使用「另存為」。" : \
    u8"Current file path is invalid, please use Save As.")

#define UI_MSG_SAVE_SUCCESS(l) ((l) == 1 ? u8"保存成功。" : \
    (l) == 2 ? u8"Save successful." : \
    (l) == 3 ? u8"保存成功。" : \
    (l) == 4 ? u8"저장 성공." : \
    (l) == 5 ? u8"บันทึกสำเร็จ." : \
    (l) == 6 ? u8"Lưu thành công." : \
    (l) == 7 ? u8"保存成功。" : \
    u8"Save successful.")

#define UI_MSG_SAVE_FAILED(l) ((l) == 1 ? u8"保存失败。" : \
    (l) == 2 ? u8"Save failed." : \
    (l) == 3 ? u8"保存に失敗しました。" : \
    (l) == 4 ? u8"저장 실패." : \
    (l) == 5 ? u8"บันทึกล้มเหลว" : \
    (l) == 6 ? u8"Lưu thất bại." : \
    (l) == 7 ? u8"儲存失敗。" : \
    u8"Save failed.")

#define UI_MSG_SAVEAS_SUCCESS(l) ((l) == 1 ? u8"另存为成功。" : \
(l) == 2 ? u8"Save As successful." : \
(l) == 3 ? u8"名前を付けて保存に成功しました。" : \
(l) == 4 ? u8"다른 이름으로 저장 성공." : \
(l) == 5 ? u8"บันทึกเป็นสำเร็จ" : \
(l) == 6 ? u8"Lưu dưới dạng thành công." : \
(l) == 7 ? u8"另存為成功。" : \
u8"Save As successful.")

#define UI_MSG_SAVEAS_FAILED(l) ((l) == 1 ? u8"另存为失败。" : \
(l) == 2 ? u8"Save As failed." : \
(l) == 3 ? u8"名前を付けて保存に失敗しました。" : \
(l) == 4 ? u8"다른 이름으로 저장 실패." : \
(l) == 5 ? u8"บันทึกเป็นล้มเหลว" : \
(l) == 6 ? u8"Lưu dưới dạng thất bại." : \
(l) == 7 ? u8"另存為失敗。" : \
u8"Save As failed.")

// Format helpers
#define UI_MENU_FMT(l) ((l) == 1 ? u8"菜单 (%u)" : \
    (l) == 2 ? u8"Menu (%u)" : \
    (l) == 3 ? u8"メニュー (%u)" : \
    (l) == 4 ? u8"메뉴 (%u)" : \
    (l) == 5 ? u8"เมนู (%u)" : \
    (l) == 6 ? u8"Menu (%u)" : \
    (l) == 7 ? u8"菜单 (%u)" : \
    u8"Menu (%u)")

#define UI_ITEM_FMT(l) ((l) == 1 ? u8"项 (%u)" : \
    (l) == 2 ? u8"Item (%u)" : \
    (l) == 3 ? u8"項目 (%u)" : \
    (l) == 4 ? u8"항목 (%u)" : \
    (l) == 5 ? u8"รายการ (%u)" : \
    (l) == 6 ? u8"Mục (%u)" : \
    (l) == 7 ? u8"项 (%u)" : \
    u8"Item (%u)")

#define UI_TOOLBAR_FMT(l) ((l) == 1 ? u8"工具栏 (%u)" : \
    (l) == 2 ? u8"Toolbar (%u)" : \
    (l) == 3 ? u8"ツールバー (%u)" : \
    (l) == 4 ? u8"도구 모음 (%u)" : \
    (l) == 5 ? u8"แถบเครื่องมือ (%u)" : \
    (l) == 6 ? u8"Thanh công cụ (%u)" : \
    (l) == 7 ? u8"工具栏 (%u)" : \
    u8"Toolbar (%u)")

// 属性列表控件本地化字符串
#define UI_PROPLIST_YES(l) ((l) == 1 ? u8"是" : \
    (l) == 2 ? u8"Yes" : \
    (l) == 3 ? u8"はい" : \
    (l) == 4 ? u8"예" : \
    (l) == 5 ? u8"ใช่" : \
    (l) == 6 ? u8"Có" : \
    (l) == 7 ? u8"是" : \
    u8"Yes")

#define UI_PROPLIST_NO(l) ((l) == 1 ? u8"否" : \
    (l) == 2 ? u8"No" : \
    (l) == 3 ? u8"いいえ" : \
    (l) == 4 ? u8"아니오" : \
    (l) == 5 ? u8"ไม่" : \
    (l) == 6 ? u8"Không" : \
    (l) == 7 ? u8"否" : \
    u8"No")

#define UI_PROPLIST_BASIC_INFO(l) ((l) == 1 ? u8"基本信息" : \
    (l) == 2 ? u8"Basic Info" : \
    (l) == 3 ? u8"基本情報" : \
    (l) == 4 ? u8"기본 정보" : \
    (l) == 5 ? u8"ข้อมูลพื้นฐาน" : \
    (l) == 6 ? u8"Thông tin cơ bản" : \
    (l) == 7 ? u8"基本資訊" : \
    u8"Basic Info")

#define UI_PROPLIST_TYPE(l) ((l) == 1 ? u8"类型" : \
    (l) == 2 ? u8"Type" : \
    (l) == 3 ? u8"タイプ" : \
    (l) == 4 ? u8"유형" : \
    (l) == 5 ? u8"ประเภท" : \
    (l) == 6 ? u8"Loại" : \
    (l) == 7 ? u8"類型" : \
    u8"Type")

#define UI_PROPLIST_TITLE(l) ((l) == 1 ? u8"标题" : \
    (l) == 2 ? u8"Title" : \
    (l) == 3 ? u8"タイトル" : \
    (l) == 4 ? u8"제목" : \
    (l) == 5 ? u8"หัวข้อ" : \
    (l) == 6 ? u8"Tiêu đề" : \
    (l) == 7 ? u8"標題" : \
    u8"Title")

#define UI_PROPLIST_CLASSNAME(l) ((l) == 1 ? u8"类名" : \
    (l) == 2 ? u8"Class Name" : \
    (l) == 3 ? u8"クラス名" : \
    (l) == 4 ? u8"클래스명" : \
    (l) == 5 ? u8"ชื่อคลาส" : \
    (l) == 6 ? u8"Tên lớp" : \
    (l) == 7 ? u8"類名" : \
    u8"Class Name")

#define UI_PROPLIST_POSITION(l) ((l) == 1 ? u8"位置(DLU)" : \
    (l) == 2 ? u8"Position(DLU)" : \
    (l) == 3 ? u8"位置(DLU)" : \
    (l) == 4 ? u8"위치(DLU)" : \
    (l) == 5 ? u8"ตำแหน่ง(DLU)" : \
    (l) == 6 ? u8"Vị trí(DLU)" : \
    (l) == 7 ? u8"位置(DLU)" : \
    u8"Position(DLU)")

#define UI_PROPLIST_LEVEL_SETTINGS(l) ((l) == 1 ? u8"等级设置" : \
    (l) == 2 ? u8"Level Settings" : \
    (l) == 3 ? u8"レベル設定" : \
    (l) == 4 ? u8"레벨 설정" : \
    (l) == 5 ? u8"การตั้งค่าระดับ" : \
    (l) == 6 ? u8"Cài đặt cấp độ" : \
    (l) == 7 ? u8"等級設定" : \
    u8"Level Settings")

#define UI_PROPLIST_IN_LEVEL_MANAGER(l) ((l) == 1 ? u8"参与等级管理" : \
    (l) == 2 ? u8"In Level Manager" : \
    (l) == 3 ? u8"レベル管理に参加" : \
    (l) == 4 ? u8"레벨 관리 참여" : \
    (l) == 5 ? u8"เข้าร่วมการจัดการระดับ" : \
    (l) == 6 ? u8"Tham gia quản lý cấp độ" : \
    (l) == 7 ? u8"參與等級管理" : \
    u8"In Level Manager")

#define UI_PROPLIST_CURRENT_LEVEL_ENABLED(l) ((l) == 1 ? u8"当前等级启用" : \
    (l) == 2 ? u8"Current Level Enabled" : \
    (l) == 3 ? u8"現在のレベル有効" : \
    (l) == 4 ? u8"현재 레벨 활성화" : \
    (l) == 5 ? u8"เปิดใช้งานระดับปัจจุบัน" : \
    (l) == 6 ? u8"Bật cấp độ hiện tại" : \
    (l) == 7 ? u8"當前等級啟用" : \
    u8"Current Level Enabled")

#define UI_PROPLIST_MULTI_SELECT(l) ((l) == 1 ? u8"多选控件" : \
    (l) == 2 ? u8"Multiple Controls" : \
    (l) == 3 ? u8"複数コントロール" : \
    (l) == 4 ? u8"다중 컨트롤" : \
    (l) == 5 ? u8"หลายตัวควบคุม" : \
    (l) == 6 ? u8"Nhiều điều khiển" : \
    (l) == 7 ? u8"多選控件" : \
    u8"Multiple Controls")

#define UI_PROPLIST_SELECT_COUNT(l) ((l) == 1 ? u8"选中数量" : \
    (l) == 2 ? u8"Selected Count" : \
    (l) == 3 ? u8"選択数" : \
    (l) == 4 ? u8"선택된 수" : \
    (l) == 5 ? u8"จำนวนที่เลือก" : \
    (l) == 6 ? u8"Số lượng đã chọn" : \
    (l) == 7 ? u8"選中數量" : \
    u8"Selected Count")

#define UI_PROPLIST_NONE(l) ((l) == 1 ? u8"无" : \
    (l) == 2 ? u8"None" : \
    (l) == 3 ? u8"なし" : \
    (l) == 4 ? u8"없음" : \
    (l) == 5 ? u8"ไม่มี" : \
    (l) == 6 ? u8"Không" : \
    (l) == 7 ? u8"無" : \
    u8"None")

#define UI_PROPLIST_TOOLBAR(l) ((l) == 1 ? u8"工具栏" : \
    (l) == 2 ? u8"Toolbar" : \
    (l) == 3 ? u8"ツールバー" : \
    (l) == 4 ? u8"도구 모음" : \
    (l) == 5 ? u8"แถบเครื่องมือ" : \
    (l) == 6 ? u8"Thanh công cụ" : \
    (l) == 7 ? u8"工具欄" : \
    u8"Toolbar")

#define UI_PROPLIST_TOOLBAR_ID(l) ((l) == 1 ? u8"工具栏ID" : \
    (l) == 2 ? u8"Toolbar ID" : \
    (l) == 3 ? u8"ツールバーID" : \
    (l) == 4 ? u8"도구 모음 ID" : \
    (l) == 5 ? u8"รหัสแถบเครื่องมือ" : \
    (l) == 6 ? u8"ID thanh công cụ" : \
    (l) == 7 ? u8"工具欄ID" : \
    u8"Toolbar ID")

#define UI_PROPLIST_BUTTON_INFO(l) ((l) == 1 ? u8"按钮信息" : \
    (l) == 2 ? u8"Button Info" : \
    (l) == 3 ? u8"ボタン情報" : \
    (l) == 4 ? u8"버튼 정보" : \
    (l) == 5 ? u8"ข้อมูลปุ่ม" : \
    (l) == 6 ? u8"Thông tin nút" : \
    (l) == 7 ? u8"按鈕資訊" : \
    u8"Button Info")

#define UI_PROPLIST_BUTTON_ID(l) ((l) == 1 ? u8"按钮ID" : \
    (l) == 2 ? u8"Button ID" : \
    (l) == 3 ? u8"ボタンID" : \
    (l) == 4 ? u8"버튼 ID" : \
    (l) == 5 ? u8"รหัสปุ่ม" : \
    (l) == 6 ? u8"ID nút" : \
    (l) == 7 ? u8"按鈕ID" : \
    u8"Button ID")

#define UI_PROPLIST_BUTTON_TEXT(l) ((l) == 1 ? u8"按钮文本" : \
    (l) == 2 ? u8"Button Text" : \
    (l) == 3 ? u8"ボタンテキスト" : \
    (l) == 4 ? u8"버튼 텍스트" : \
    (l) == 5 ? u8"ข้อความปุ่ม" : \
    (l) == 6 ? u8"Văn bản nút" : \
    (l) == 7 ? u8"按鈕文本" : \
    u8"Button Text")

#define UI_PROPLIST_MULTI_BUTTONS(l) ((l) == 1 ? u8"多选按钮" : \
    (l) == 2 ? u8"Multiple Buttons" : \
    (l) == 3 ? u8"複数ボタン" : \
    (l) == 4 ? u8"다중 버튼" : \
    (l) == 5 ? u8"หลายปุ่ม" : \
    (l) == 6 ? u8"Nhiều nút" : \
    (l) == 7 ? u8"多選按鈕" : \
    u8"Multiple Buttons")

#define UI_PROPLIST_LEVEL_FMT(l) ((l) == 1 ? u8"等级 %d" : \
    (l) == 2 ? u8"Level %d" : \
    (l) == 3 ? u8"レベル %d" : \
    (l) == 4 ? u8"레벨 %d" : \
    (l) == 5 ? u8"ระดับ %d" : \
    (l) == 6 ? u8"Cấp độ %d" : \
    (l) == 7 ? u8"等級 %d" : \
    u8"Level %d")

#define UI_PROPLIST_MULTILINE(l) ((l) == 1 ? u8"多行显示" : \
    (l) == 2 ? u8"Multiline" : \
    (l) == 3 ? u8"複数行表示" : \
    (l) == 4 ? u8"여러 줄" : \
    (l) == 5 ? u8"หลายบรรทัด" : \
    (l) == 6 ? u8"Nhiều dòng" : \
    (l) == 7 ? u8"多行顯示" : \
    u8"Multiline")

// UTF-8 -> CString 的转换实现已迁移至 GovernLens/utils/TextConversion.h。
// 这里保留旧 LevelLocalize/RcCore 的全局 ML 入口，避免影响编辑器调用点。
#ifdef ML
#undef ML
#endif

static inline CString ML(const char* utf8Str)
{
    return rceditor::Utf8ToCString(utf8Str);
}

// 已核实的 ImageAVI 工具栏用途名称；不以首个按钮命名，不加工具栏后缀。
inline CString GetLevelAuditToolbarCaption(UINT id, int lan)
{
    static const struct { UINT id; const char* text[7]; } names[] =
    {
        {188, {u8"调试公共", u8"Common debug", u8"共通デバッグ", u8"공통 디버그", u8"ดีบักทั่วไป", u8"Gỡ lỗi chung", u8"調試公共"}},
        {200, {u8"对位", u8"Alignment", u8"位置合わせ", u8"정렬", u8"จัดแนว", u8"Căn chỉnh", u8"對位"}},
        {201, {u8"参数", u8"Parameters", u8"パラメータ", u8"매개변수", u8"พารามิเตอร์", u8"Tham số", u8"參數"}},
        {203, {u8"缺陷编辑", u8"Defect editing", u8"欠陥編集", u8"결함 편집", u8"แก้ไขข้อบกพร่อง", u8"Chỉnh sửa lỗi", u8"缺陷編輯"}},
        {204, {u8"区域", u8"Regions", u8"領域", u8"영역", u8"พื้นที่", u8"Vùng", u8"區域"}},
        {244, {u8"RGB 学习", u8"RGB learning", u8"RGB 学習", u8"RGB 학습", u8"เรียนรู้ RGB", u8"Học RGB", u8"RGB 學習"}},
        {245, {u8"学板", u8"Board learning", u8"基板学習", u8"기판 학습", u8"เรียนรู้แผงวงจร", u8"Học bo mạch", u8"學板"}},
        {483, {u8"距离测量", u8"Distance measurement", u8"距離測定", u8"거리 측정", u8"วัดระยะทาง", u8"Đo khoảng cách", u8"距離測量"}},
        {487, {u8"特殊焊盘", u8"Special pads", u8"特殊パッド", u8"특수 패드", u8"แพดพิเศษ", u8"Pad đặc biệt", u8"特殊焊盤"}},
        {539, {u8"自动设色对位", u8"Auto-color alignment", u8"自動色設定の位置合わせ", u8"자동 색상 정렬", u8"จัดแนวสีอัตโนมัติ", u8"Căn chỉnh màu tự động", u8"自動設色對位"}}
    };
    const int index = lan >= 1 && lan <= 7 ? lan - 1 : 1;
    for (const auto& item : names)
        if (item.id == id)
            return ML(item.text[index]);
    return CString();
}

#endif

/* ===================== 原文件：LevelAuditIgnore.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
// [已合并依赖] #include "RcCore.h"
// [已合并依赖] #include "DebugPageLevel.h"
// [已合并依赖] #include "ToolHistogramLevel.h"
#include <algorithm>
// [已合并依赖] #include "LevelLocalize.h"

// 按模块基本名匹配，兼容 EXE、DLL 及语言资源前缀。
inline bool IsLevelAuditModule(const CString& filePath, LPCTSTR name)
{
    // 取模块基本名，兼容 EXE、DLL 以及 En_ImageAVI 等语言资源模块。
    CString module = filePath;
    module.Replace(_T('/'), _T('\\'));
    module = module.Mid(module.ReverseFind(_T('\\')) + 1);
    int dot = module.ReverseFind(_T('.'));
    if (dot >= 0)
        module = module.Left(dot);

    CString suffix = CString(_T("_")) + name;
    return module.CompareNoCase(name) == 0 || module.Right(suffix.GetLength()).CompareNoCase(suffix) == 0;
}

inline CString GetLevelResourceModuleName(const CString& filePath)
{
    CString module = filePath;
    module.Replace(_T('/'), _T('\\'));
    module = module.Mid(module.ReverseFind(_T('\\')) + 1);
    const int dot = module.ReverseFind(_T('.'));
    if (dot >= 0 && (module.Mid(dot).CompareNoCase(_T(".dll")) == 0 ||
        module.Mid(dot).CompareNoCase(_T(".exe")) == 0)) module = module.Left(dot);
    const LPCTSTR prefixes[] = {_T("En_"), _T("Ja_"), _T("Th_"), _T("Vi_"), _T("Tr_")};
    for (LPCTSTR prefix : prefixes)
    {
        const int length = lstrlen(prefix);
        if (module.Left(length).CompareNoCase(prefix) == 0)
        {
            module = module.Mid(length);
            break;
        }
    }
    module.MakeLower();
    return module;
}

inline bool IsSameLevelResourceModule(const CString& filePath, const std::string& jsonName)
{
    if (jsonName.empty()) return false;
    const CString name = rceditor::CStringConverter::ConvertToT(
        rceditor::CStringConverter::FromUtf8(jsonName));
    return GetLevelResourceModuleName(filePath) == GetLevelResourceModuleName(name);
}

// 登录是切换用户的入口，不参与等级管控；限定模块和工具栏，避免资源 ID 重号。
inline bool IsLevelExemptToolbarItem(const CString& filePath, UINT toolbarId, UINT itemId)
{
    return IsLevelAuditModule(filePath, _T("ImageAVI")) &&
        toolbarId == 188 && itemId == 32814;
}

// 等级管理入树前调用：名称为空或命中 ignore 的窗口不加入列表。
// 这里只过滤显示，不修改资源、等级掩码及保存的权限配置。
inline bool ShouldIgnoreLevelAuditDialog(const CString& filePath, const rceditor::SDialogInfo& dlg)
{
    CStringW caption = dlg.csCaption;
    caption.Trim();
    if (caption.IsEmpty())
        return true;

    // 动态窗口与静态资源可能重号，下面的 ignore 仅针对静态对话框。
    if (dlg.m_blIsDynamic)
        return false;

    // 研发新增排除项只需加一行；同步其他软件时先核对该软件的资源 ID。
    // 按模块和 ID 匹配，避免不同模块重号，也不依赖翻译后的窗口标题。
    static const struct { LPCTSTR module; UINT id; } ignore[] =
    {
        { _T("AviBaseClass"), 106 },  // 数量
        { _T("AviBaseClass"), 158 },  // 输入名称
        { _T("AviBaseClass"), 235 },  // 参数级别
        { _T("AviBaseClass"), 1001 },  // 消息
        { _T("AviBaseClass"), 1002 },  // 用户编辑
        { _T("AviBaseClass"), 1003 },  // 登录
        { _T("AviBaseClass"), 1004 },  // 修改密码
        { _T("AviBaseClass"), 1023 },
        { _T("AviBaseClass"), 1035 },
        { _T("AviBaseClass"), 1036 },
        { _T("AviBaseClass"), 1040 },
        { _T("AviBaseClass"), 1042 },
        { _T("AviBaseClass"), 1045 },
        { _T("AviBaseClass"), 1047 },
        { _T("AviBaseClass"), 1050 },
        { _T("AviBaseClass"), 1053 },
        { _T("AviBaseClass"), 1058 },
        { _T("AviBaseClass"), 1061 },
        { _T("AviBaseClass"), 1063 },
        { _T("AviBaseClass"), 1067 },
        { _T("AviBaseClass"), 1069 },
        { _T("AviBaseClass"), 1072 },
        { _T("AviBaseClass"), 1074 },
        { _T("AviBaseClass"), 1077 },
        { _T("AviBaseClass"), 1080 },
        { _T("AviBaseClass"), 1083 },
        { _T("AviBaseClass"), 1086 },
        { _T("AviBaseClass"), 1089 },
        { _T("AviBaseClass"), 1092 },
        { _T("AviBaseClass"), 1095 },
        { _T("AviBaseClass"), 1098 },
        { _T("AviBaseClass"), 3000 },  // 缺陷统计
        { _T("AviBaseClass"), 3001 },  // 时间
        { _T("ImageAVI"), 100 },
        { _T("ImageAVI"), 102 },
        { _T("ImageAVI"), 120 },
        { _T("ImageAVI"), 137 },  // 旧资料转换
        { _T("ImageAVI"), 143 },  // 密码
        { _T("ImageAVI"), 144 },  // 导出历史记录
        { _T("ImageAVI"), 145 },  // 加载资料
        { _T("ImageAVI"), 146 },  // 设置批量号
        { _T("ImageAVI"), 147 },  // 设置面
        { _T("ImageAVI"), 148 },  // 上下台面速度（调试）
        { _T("ImageAVI"), 155 },  // 伸缩率
        { _T("ImageAVI"), 158 },  // 输入名称
        { _T("ImageAVI"), 163 },  // 自动抓板
        { _T("ImageAVI"), 164 },  // 颜色过滤
        { _T("ImageAVI"), 166 },  // 自动调焦
        { _T("ImageAVI"), 167 },
        { _T("ImageAVI"), 184 },
        { _T("ImageAVI"), 205 },  // 扫描列
        { _T("ImageAVI"), 228 },  // 绿油偏位点
        { _T("ImageAVI"), 229 },  // 离线扫描
        { _T("ImageAVI"), 230 },  // 缺陷过滤
        { _T("ImageAVI"), 234 },  // 保存图像
        { _T("ImageAVI"), 247 },  // 新建资料
        { _T("ImageAVI"), 248 },  // 更改为
        { _T("ImageAVI"), 250 },  // 多片测试排版
        { _T("ImageAVI"), 256 },
        { _T("ImageAVI"), 297 },  // 另存为
        { _T("ImageAVI"), 298 },  // 修改密码
        { _T("ImageAVI"), 368 },
        { _T("ImageAVI"), 369 },  // 离线指定扫描板号
        { _T("ImageAVI"), 370 },  // 显示预览图
        { _T("ImageAVI"), 404 },
        { _T("ImageAVI"), 489 },  // 学板
        { _T("ImageAVI"), 494 },
        { _T("ImageAVI"), 536 },
        { _T("ImageAVI"), 548 },
        { _T("ImageAVI"), 1002 },  // 更新测试数据
        { _T("ImageAVI"), 1032 },  // 选择参数范围文件
        { _T("ImageAVI"), 1485 },  // 选择CCD
        { _T("ImageAVI"), 1508 },  // 学习-颜色
        { _T("ImageAVI"), 1514 },  // 学习-颜色
        { _T("ImageAVI"), 1519 },  // 学习-颜色
        { _T("ImageAVI"), 3002 },  // 一般
        { _T("ImageAVI"), 3003 },  // 线路
        { _T("ImageAVI"), 3004 },  // 缺陷大小
        { _T("ImageAVI"), 3006 },  // 颜色
        { _T("ImageAVI"), 3007 },  // 焊盘
        { _T("ImageAVI"), 3008 },  // 孔和孔环
        { _T("ImageAVI"), 3018 },  // 相机状态
        { _T("ImageAVI"), 3691 },  // EAP信息提示
        { _T("ImageAVI"), 3694 },  // 密码
        { _T("ImageAVI"), 3695 },  // 线路2
        { _T("ImageAVI"), 3721 },  // 线路微凹陷
        { _T("ImageAVI"), 62000 },  // 特殊料号提示设置
        { _T("ImageAVI"), 62001 },  // 特殊料号提示
        { _T("ImageAVI"), 62300 },  // 超时下线
        { _T("ImageAVI"), 62301 },  // 账号即将下线
        { _T("ImageAVI"), 62318 },  // 重置登录时间
    };
    for (const auto& item : ignore)
    {
        if (dlg.nIDD == item.id && IsLevelAuditModule(filePath, item.module))
            return true;
    }
    return false;
}

// 只过滤资源本身禁用的控件；等级掩码为零的控件仍需显示，供管理员配置。
inline bool IsLevelAuditControlHidden(const rceditor::SCtrlInfo& ctrl)
{
    return (ctrl.dwStyle & WS_DISABLED) != 0;
}

struct LevelAuditDocPage
{
    UINT id;
    UINT parentId; // 0 表示主 Tab
};

inline const LevelAuditDocPage* GetLevelAuditDocPage(UINT id, UINT* order = nullptr)
{
    static const LevelAuditDocPage pages[] =
    {
        {3697, 0}, {106, 0}, {154, 0}, {1498, 0}, {1007, 0},
        {260, 0}, {253, 0}, {1504, 0}, {1491, 0}, {3722, 0},
        {504, 0}, {3723, 0}, {1488, 0}, {1480, 0}, {426, 0},
        {399, 154}, {406, 1498}, {407, 1504}, {409, 1007}, {491, 1007},
        {149, 1480}, {3065, 1480}, {3692, 1480}, {3696, 1480}
    };
    for (UINT i = 0; i < sizeof(pages) / sizeof(pages[0]); ++i)
        if (pages[i].id == id)
        {
            if (order) *order = i;
            return &pages[i];
        }
    return nullptr;
}

inline bool IsLevelAuditDocSettingDialog(const CString& filePath, const rceditor::SDialogInfo& dlg)
{
    if (!IsLevelAuditModule(filePath, _T("ImageAVI")))
        return false;
    if (dlg.m_blIsDynamic)
        return dlg.nIDD == 0xF000; // 文档设定的保存到库、从库选择、复制按钮
    return GetLevelAuditDocPage(dlg.nIDD) != nullptr;
}

// 只在 ImageAVI 等级树中使用业务顺序；其它模块仍按资源 ID 排序。
inline UINT GetLevelAuditDocPageOrder(const CString& filePath, const rceditor::SDialogInfo* dlg)
{
    UINT order = ~0u;
    if (dlg && !dlg->m_blIsDynamic && IsLevelAuditModule(filePath, _T("ImageAVI")))
        GetLevelAuditDocPage(dlg->nIDD, &order);
    return order;
}

// 业务分类只改变树的组织；叶节点仍保留模块路径和资源 ID，用于原有 JSON。
enum ELevelAuditGroup
{
    LevelGroupDocument = 0xF000,
    LevelGroupToolbars = 0xF002,
    LevelGroupAdvanced = 0xF003,
    LevelGroupLight = 0xF004,
    LevelGroupColor = 0xF005,
    LevelGroupOther = 0xF006,
    LevelGroupDebugPages = 0xF007
};

inline UINT GetLevelAuditDialogGroup(const CString& filePath, const rceditor::SDialogInfo& dlg,
    UINT* order = nullptr)
{
    if (order) *order = dlg.nIDD + 100;
    if (IsLevelAuditDocSettingDialog(filePath, dlg))
    {
        if (order) *order = dlg.m_blIsDynamic ? 99 : GetLevelAuditDocPageOrder(filePath, &dlg);
        return LevelGroupDocument;
    }
    if (dlg.m_blIsDynamic)
    {
        const int page = DebugPageLevel::PageOrder(dlg.nIDD);
        if (page >= 0 && IsLevelAuditModule(filePath, _T("ImageAVI")))
        {
            if (order) *order = (UINT)page;
            return LevelGroupDebugPages;
        }
        return LevelGroupOther;
    }
    if (IsLevelAuditModule(filePath, _T("AviBaseClass")) && dlg.nIDD == 157)
    {
        if (order) *order = 1;
        return LevelGroupColor;
    }
    if (!IsLevelAuditModule(filePath, _T("ImageAVI"))) return LevelGroupOther;
    static const struct { UINT id; UINT group; UINT rank; } pages[] =
    {
        {202, LevelGroupAdvanced, 0},
        {239, LevelGroupAdvanced, 1},   // Gerber 颜色：从高级设定打开，优先按入口归属
        {477, LevelGroupAdvanced, 2},
        {532, LevelGroupAdvanced, 3},   // CPK
        {61959, LevelGroupAdvanced, 4}, // 数据库配置
        {571, LevelGroupAdvanced, 5},   // 离线更新机器 IP
        {555, LevelGroupAdvanced, 6},   // AI 设置
        {734, LevelGroupAdvanced, 7},   // AI 模型设置
        {476, LevelGroupLight, 0},      // 图示：抽帧方式光源亮度调节
        {3693, LevelGroupLight, 1},     // 自动校光
        {244, LevelGroupLight, 2},      // 验光
        {245, LevelGroupLight, 3},      // 验光设定
        {178, LevelGroupColor, 0},      // 图示：颜色模板编辑，必须第一项
        {ToolHistogramLevel::ColorDialog, LevelGroupToolbars, 0}, // 颜色统计
        {ToolHistogramLevel::StudyDialog, LevelGroupToolbars, 1}, // 学板
        {185, LevelGroupColor, 3},      // 学习颜色
        {534, LevelGroupColor, 4},      // 自动设置颜色
        {3724, LevelGroupColor, 5},     // 自动颜色模式
        {539, LevelGroupColor, 6}       // 颜色替换
    };
    for (const auto& page : pages)
        if (page.id == dlg.nIDD)
        {
            if (order) *order = page.rank;
            return page.group;
        }
    return LevelGroupOther;
}

inline UINT GetLevelAuditToolbarPage(const CString& filePath, UINT toolbarId)
{
    return IsLevelAuditModule(filePath, _T("ImageAVI")) ? DebugPageLevel::ToolbarPage(toolbarId) : 0;
}

inline void RemoveLevelAuditNavigation(const CString& filePath, rceditor::SDialogInfo& dialog)
{
    if (!dialog.m_blIsDynamic || !IsLevelAuditModule(filePath, _T("ImageAVI")) ||
        dialog.nIDD != DebugPageLevel::Region) return;
    auto& controls = dialog.vecCtrlInfo;
    controls.erase(std::remove_if(controls.begin(), controls.end(), [&](const std::shared_ptr<rceditor::SCtrlInfo>& control) {
        return control && DebugPageLevel::IsLegacyNavigation(dialog.nIDD, control->nIDC);
    }), controls.end());
}

inline const char* GetLevelAuditGroupText(UINT group, unsigned short language)
{
    switch (group)
    {
    case LevelGroupDebugPages: return LEVEL_GROUP_DEBUG_PAGES(language);
    case LevelGroupDocument: return LEVEL_GROUP_DOCUMENT(language);
    case LevelGroupAdvanced: return LEVEL_GROUP_ADVANCED(language);
    case LevelGroupLight: return LEVEL_GROUP_LIGHT(language);
    case LevelGroupColor: return LEVEL_GROUP_COLOR(language);
    case LevelGroupToolbars: return UI_TOOLBARS(language);
    default: return UI_DIALOGS(language);
    }
}

// AVI635D 中无有效加载入口的旧工具栏，仅从等级配置列表隐藏。
// 保留资源和 JSON 权限数据；其他模块以及未来新增的工具栏不受影响。
inline bool ShouldIgnoreLevelAuditToolbar(const CString& filePath, UINT toolbarId)
{
    if (!IsLevelAuditModule(filePath, _T("ImageAVI")))
        return false;
    switch (toolbarId)
    {
    case 139:
    case 141:
    case 142:
    case 145:
    case 202:
    case 242:
    case 243:
        return true;
    default:
        return false;
    }
}

// 已知 ImageAVI 工具栏按用途命名；其他资源仍沿用原有文字回退。
inline CString GetLevelAuditToolbarText(const CString& filePath,
    const rceditor::SToolbarInfo& toolbar, const CString& fallback)
{
    if (IsLevelAuditModule(filePath, _T("ImageAVI")))
    {
        CString caption = GetLevelAuditToolbarCaption(toolbar.nToolbarID, gbCurNeedShowLan);
        if (!caption.IsEmpty()) return caption;
    }
    for (const auto& value : toolbar.vecItemText)
    {
        CString text(value);
        int newline = text.Find(_T('\n'));
        if (newline >= 0) text = text.Mid(newline + 1);
        text.Trim();
        if (!text.IsEmpty()) return text;
    }
    return fallback;
}

inline void MigrateToolHistogramMasks(UINT dialogId, rceditor::SDialogInfo& dialog,
    const std::vector<std::pair<UINT, UINT>>& savedMasks)
{
    if (dialogId != ToolHistogramLevel::ColorDialog && dialogId != ToolHistogramLevel::StudyDialog) return;
    auto legacy = std::find_if(savedMasks.begin(), savedMasks.end(), [](const std::pair<UINT, UINT>& item) {
        return item.first == ToolHistogramLevel::Histogram;
    });
    if (legacy == savedMasks.end()) return;
    for (auto& ctrl : dialog.vecCtrlInfo)
    {
        if (!ctrl || ctrl->nIDC < ToolHistogramLevel::FirstChannel ||
            ctrl->nIDC >= ToolHistogramLevel::FirstChannel + ToolHistogramLevel::ChannelCount) continue;
        auto explicitMask = std::find_if(savedMasks.begin(), savedMasks.end(), [&](const std::pair<UINT, UINT>& item) {
            return item.first == ctrl->nIDC;
        });
        if (explicitMask == savedMasks.end()) ctrl->uiLevelMask = legacy->second;
    }
}

inline void EnsureToolHistogramControls(
    std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>& resources)
{
    const wchar_t* caption = L"Histogram";
    switch (rceditor::gbCurNeedShowLan)
    {
    case 1: caption = L"\u76f4\u65b9\u56fe"; break;
    case 7: caption = L"\u76f4\u65b9\u5716"; break;
    case 3: caption = L"\u30d2\u30b9\u30c8\u30b0\u30e9\u30e0"; break;
    case 4: caption = L"\ud788\uc2a4\ud1a0\uadf8\ub7a8"; break;
    case 5: caption = L"\u0e2e\u0e34\u0e2a\u0e42\u0e15\u0e41\u0e01\u0e23\u0e21"; break;
    case 6: caption = L"Bi\u1ec3u \u0111\u1ed3 t\u1ea7n su\u1ea5t"; break;
    }
    for (auto& resource : resources)
    {
        if (!resource.second || !IsLevelAuditModule(resource.first, _T("ImageAVI"))) continue;
        for (UINT id : {UINT(ToolHistogramLevel::ColorDialog), UINT(ToolHistogramLevel::StudyDialog)})
        {
            auto entry = resource.second->mapLangAndDialogInfo.find(id);
            if (entry == resource.second->mapLangAndDialogInfo.end() || !entry->second) continue;
            auto& controls = entry->second->vecCtrlInfo;
            std::vector<std::pair<UINT, UINT>> savedMasks;
            for (const auto& ctrl : controls)
                if (ctrl) savedMasks.emplace_back(ctrl->nIDC, ctrl->uiLevelMask);
            controls.erase(std::remove_if(controls.begin(), controls.end(), [](const std::shared_ptr<rceditor::SCtrlInfo>& ctrl) {
                return ctrl && ctrl->nIDC == ToolHistogramLevel::Histogram;
            }), controls.end());
            const wchar_t* labels[] = {L"R", L"G", L"B", L"UNI", L"R/G", L"R/B", L"G/B"};
            if (rceditor::gbCurNeedShowLan == 1) labels[3] = L"\u5747\u5300\u6027 (UNI)";
            if (rceditor::gbCurNeedShowLan == 2) labels[3] = L"Uniformity (UNI)";
            if (rceditor::gbCurNeedShowLan == 3) labels[3] = L"\u5747\u4e00\u6027 (UNI)";
            if (rceditor::gbCurNeedShowLan == 5) labels[3] = L"\u0e04\u0e27\u0e32\u0e21\u0e2a\u0e21\u0e48\u0e33\u0e40\u0e2a\u0e21\u0e2d (UNI)";
            if (rceditor::gbCurNeedShowLan == 6) labels[3] = L"\u0110\u1ed9 \u0111\u1ed3ng \u0111\u1ec1u (UNI)";
            if (rceditor::gbCurNeedShowLan == 7) labels[3] = L"\u5747\u52fb\u6027 (UNI)";
            for (int channel = 0; channel < ToolHistogramLevel::ChannelCount; ++channel)
            {
                const UINT controlId = ToolHistogramLevel::ChannelControlId(channel);
                auto found = std::find_if(controls.begin(), controls.end(), [controlId](const std::shared_ptr<rceditor::SCtrlInfo>& ctrl) {
                    return ctrl && ctrl->nIDC == controlId;
                });
                std::shared_ptr<rceditor::SCtrlInfo> ctrl;
                if (found != controls.end()) ctrl = *found;
                else
                {
                    ctrl = std::make_shared<rceditor::SCtrlInfo>();
                    ctrl->nIDC = controlId;
                    ctrl->uiLevelMask = ~UINT(0);
                    controls.push_back(ctrl);
                }
                ctrl->csClassName = L"Static";
                ctrl->dwStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY | SS_CENTER | SS_CENTERIMAGE;
                ctrl->csText = CStringW(labels[channel]) + L" - " + caption;
                const bool colorDialog = id == ToolHistogramLevel::ColorDialog;
                const int left = colorDialog ? 218 : 127;
                const int right = colorDialog ? 460 : 442;
                const int step = colorDialog ? 40 : 44;
                const int top = 7 + channel * step;
                ctrl->ctrlRect = CRect(left, top, right, top + step - 4);
            }
            MigrateToolHistogramMasks(id, *entry->second, savedMasks);
        }
    }
}


/*
==================================================================================
                                三、等级编辑器与设计界面
==================================================================================
*/

#include "../ui/ParameterEditorStyle.h"

/* ===================== 原文件：CPropertyListCtrl.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]

#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include <functional>

/*!
 * \brief 属性列表控件，用于替代CMFCPropertyGridCtrl
 * \details 使用CListCtrl实现简单的属性-值两列显示
 *          支持分类行（灰色背景）和普通属性行
 */
class CPropertyListCtrl : public CListCtrl
{
    DECLARE_DYNAMIC(CPropertyListCtrl)

public:
    /*!
     * \brief 属性项数据结构
     */
    struct SPropertyItem
    {
        CString strName;           // 属性名称
        CString strValue;          // 属性值
        DWORD_PTR dwData;          // 用户自定义数据（属性ID等）
        bool bIsCategory;          // 是否为分类行
        bool bReadOnly;            // 是否只读
        bool bIsBool;              // 是否为布尔类型
        bool bBoolValue;           // 布尔值（当bIsBool为true时有效）
        bool bIsText;              // 是否为可编辑文本（双击进入编辑模式）

        SPropertyItem()
            : dwData(0)
            , bIsCategory(false)
            , bReadOnly(false)
            , bIsBool(false)
            , bBoolValue(false)
            , bIsText(false)
        {}
    };

    /*!
     * \brief 属性值变更回调类型
     * \param nIndex 列表项索引
     * \param item 属性项数据
     */
    using PropertyChangedCallback = std::function<void(int nIndex, const SPropertyItem& item)>;

public:
    CPropertyListCtrl();
    virtual ~CPropertyListCtrl();

    /*!
     * \brief 初始化列表控件（创建列等）
     * \param strNameHeader 属性列标题
     * \param strValueHeader 值列标题
     */
    void InitColumns(const CString& strNameHeader = _T("属性"), const CString& strValueHeader = _T("值"));

    /*!
     * \brief 清空所有属性项
     */
    void RemoveAllProperties();

    /*!
     * \brief 添加分类行
     * \param strCategoryName 分类名称
     * \return 新增行索引
     */
    int AddCategory(const CString& strCategoryName);

    /*!
     * \brief 添加属性项
     * \param strName 属性名
     * \param strValue 属性值
     * \param dwData 用户数据
     * \param bReadOnly 是否只读
     * \param bIsText 是否为可编辑文本（双击进入编辑模式）
     * \return 新增行索引
     */
    int AddProperty(const CString& strName, const CString& strValue, DWORD_PTR dwData = 0, bool bReadOnly = false, bool bIsText = false);

    /*!
     * \brief 添加布尔属性项
     * \param strName 属性名
     * \param bValue 布尔值
     * \param dwData 用户数据
     * \param bReadOnly 是否只读
     * \return 新增行索引
     */
    int AddBoolProperty(const CString& strName, bool bValue, DWORD_PTR dwData = 0, bool bReadOnly = false);

    /*!
     * \brief 设置布尔值的本地化显示字符串
     * \param strYes "是"的本地化字符串
     * \param strNo "否"的本地化字符串
     */
    void SetBoolLabels(const CString& strYes, const CString& strNo);

    /*!
     * \brief 获取指定行的属性数据
     * \param nIndex 行索引
     * \param outItem 输出的属性数据
     * \return 是否成功
     */
    bool GetPropertyItem(int nIndex, SPropertyItem& outItem) const;

    /*!
     * \brief 更新指定行的值
     * \param nIndex 行索引
     * \param strValue 新值
     */
    void SetPropertyValue(int nIndex, const CString& strValue);

    /*!
     * \brief 更新指定行的布尔值
     * \param nIndex 行索引
     * \param bValue 新布尔值
     */
    void SetPropertyBoolValue(int nIndex, bool bValue);

    /*!
     * \brief 设置属性变更回调
     * \param callback 回调函数
     */
    void SetPropertyChangedCallback(PropertyChangedCallback callback);

    /*!
     * \brief 获取属性项数量
     */
    int GetPropertyCount() const { return (int)m_vecItems.size(); }

protected:
    //! 列宽比例（属性名列占比）
    static const int NAME_COLUMN_RATIO = 40;

    //! 属性项存储
    std::vector<SPropertyItem> m_vecItems;

    //! 属性变更回调
    PropertyChangedCallback m_pfnPropertyChanged;

    //! 是否正在编辑
    bool m_bEditing;

    //! 当前编辑的行索引
    int m_nEditingRow;

    //! 编辑控件
    CEdit* m_pEditCtrl;

    //! 布尔值本地化字符串
    CString m_strBoolYes;
    CString m_strBoolNo;

protected:
    /*!
     * \brief 绘制分类行背景
     */
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 处理双击开始编辑
     */
    afx_msg void OnNMDblclk(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 处理单击切换布尔值
     */
    afx_msg void OnNMClick(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 处理控件大小变化，调整列宽
     */
    afx_msg void OnSize(UINT nType, int cx, int cy);

    /*!
     * \brief 结束编辑
     */
    void EndEdit(bool bSave);

    /*!
     * \brief 调整列宽以适应控件宽度
     */
    void AdjustColumnWidths();

    DECLARE_MESSAGE_MAP()
};


/* ===================== 原文件：TabSheet.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#if !defined(AFX_TABSHEET_H__42EE262D_D15F_46D5_8F26_28FD049E99F4__INCLUDED_)
#define AFX_TABSHEET_H__42EE262D_D15F_46D5_8F26_28FD049E99F4__INCLUDED_

#if _MSC_VER > 1000
// [原文件 pragma once：聚合后由本文件统一保护]
#endif // _MSC_VER > 1000
// TabSheet.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTabSheet window
#define MAXPAGE 16

class CTabSheet : public CTabCtrl
{
	// Construction
public:
	CTabSheet();

public:

public:

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTabSheet)
	//}}AFX_VIRTUAL

	// Implementation
public:
	int GetCurSel();
	int SetCurSel(int nItem);
	void Show();
	void SetRect();
	BOOL AddPage(LPCTSTR title, CDialog *pDialog, UINT ID);
	virtual ~CTabSheet();

	// Generated message map functions
protected:
	LPCTSTR m_Title[MAXPAGE];
	UINT m_IDD[MAXPAGE];
	CDialog* m_pPages[MAXPAGE];
	int m_nNumOfPages;
	int m_nCurrentPage;
	//{{AFX_MSG(CTabSheet)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TABSHEET_H__42EE262D_D15F_46D5_8F26_28FD049E99F4__INCLUDED_)


#include "../ui/CFormRcBase.h"

/* ===================== 原文件：CDesignSurfaceWnd.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>
#include <vector>
#include <memory>
#include <unordered_map>
// [已合并依赖] #include "RcCore.h"

#ifndef WM_APP_DESIGNSURFACE
#define WM_APP_DESIGNSURFACE (WM_APP + 210)
#endif


namespace rceditor
{
    /*!\
     * \brief 设计画布窗口类，支持对话框/控件/工具栏的可视化编辑与交互\
     * \details 提供控件选择、属性编辑、等级标记、拖拽、滚动等功能\
     */
    class CRcDesignSurfaceWnd : public CWnd
    {
    public:
        /*! \brief 构造函数 */
        explicit CRcDesignSurfaceWnd() = default;
        /*! \brief 析构函数 */
        ~CRcDesignSurfaceWnd() override;

        /*!\
         * \brief 创建设计画布窗口\n
         * \param parent 父窗口\n
         * \param id 控件ID\n
         * \param rect 初始矩形\n
         * \return 创建成功返回 TRUE
         */
        BOOL Create(CWnd* parent, UINT id, const RECT& rect);

        /*! \brief 设置当前要编辑的对话框信息（数据源：RcCore::SDialogInfo） */
        void SetDialogInfo(std::shared_ptr<SDialogInfo> dlg);
        /*! \brief 显示指定工具栏资源于设计画布 */
        void SetToolbarInfo(std::shared_ptr<SToolbarInfo> tb);
        /*! \brief 清除当前活动的工具栏预览 */
        void ClearToolbarInfo();
        /*! \brief 获取当前对话框信息 */
        std::shared_ptr<SDialogInfo> GetDialogInfo() const { return m_dialog; }
        /*! 
			\brief 选中指定控件（支持Ctrl多选）
			\param nCtrlId 控件ID
			\param ctrlDown 是否Ctrl多选
		*/
        void SelectControl(UINT nCtrlId, bool ctrlDown = false);
        /*! \brief 选中指定控件（按索引）\n \param idx 控件索引\n \param ctrlDown 是否Ctrl多选\n \param shiftDown 是否Shift多选 */
        void SelectControlByIndex(int idx, bool ctrlDown = false, bool shiftDown = false);
        /*! \brief 选中工具栏子项（按索引）\n \param idx 按钮索引\n \param ctrlDown 是否Ctrl多选\n \param shiftDown 是否Shift多选 */
        void SelectToolbarByIndex(int idx, bool ctrlDown = false, bool shiftDown = false);
        /*! \brief 清空所有选中控件 */
        void ClearSelection();

        /*! \brief 选中所有控件（用于LevelManager全选） */
        void SelectAllControls();

        /*! \brief 获取当前多选的控件索引（相对于vecCtrlInfo的索引） */
        std::vector<int> GetMultiSelectedIndices() const;
        /*! \brief 设置当前等级（1-8），用于绘制等级标记 */
        void SetCurrentLevel(int level);
        /*! \brief 获取当前等级（1-8） */
        int GetCurrentLevel() const { return m_currentLevel; }

        /*! \brief 启用/禁用等级管理器配置模式\n \param enable 是否启用 */
        void SetLevelManagerConfigMode(bool enable);
        /*! \brief 是否为等级管理器配置模式 */
        bool IsLevelManagerConfigMode() const { return m_levelManagerConfigMode; }

        // ---- 属性网格集成相关 ----
        /*! \brief 是否选中对话框 */
        bool IsDialogSelected() const { return m_dialogSelected; }
        /*! \brief 获取当前选中控件索引 */
        int GetSelectedIndex() const { return m_selectedIndex; }
        /*! \brief 获取当前选中控件指针 */
        std::shared_ptr<SCtrlInfo> GetSelectedCtrl() const
        {
            if (!m_dialog || m_selectedIndex < 0 || m_selectedIndex >= (int)m_dialog->vecCtrlInfo.size())
                return nullptr;
            return m_dialog->vecCtrlInfo[m_selectedIndex];
        }
        /*! \brief 获取当前选中控件ID */
        UINT GetSelectedCtrlId() const
        {
            auto c = GetSelectedCtrl();
            return c ? c->nIDC : 0;
        }
        /*! \brief 设置对话框标题 */
        void SetDialogCaption(const CStringW& caption);
        /*! \brief 设置控件文本 */
        void SetCtrlText(UINT ctrlId, const CStringW& text);
        /*! \brief 设置控件类名 */
        void SetCtrlClass(UINT ctrlId, const CStringW& cls);
        /*! \brief 设置控件DLU矩形 */
        void SetCtrlRectDlu(UINT ctrlId, const CRect& dluRect);
        /*! \brief 设置控件风格 */
        void SetCtrlStyle(UINT ctrlId, DWORD style);
        /*! \brief 设置控件扩展风格 */
        void SetCtrlExStyle(UINT ctrlId, DWORD exStyle);
        /*! \brief 设置控件多行属性 */
        void SetCtrlMultiLine(UINT ctrlId, bool enable);

        /*! \brief 确保指定索引控件可见（自动滚动/居中） */
        void EnsureControlVisibleByIndex(int idx);

	protected:
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void OnPaint();
		afx_msg BOOL OnEraseBkgnd(CDC* pDC);
		afx_msg void OnDestroy();
		afx_msg void OnSize(UINT nType, int cx, int cy);
		afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
		afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
		DECLARE_MESSAGE_MAP()

	private:
        /*! ---- 预览窗口缓存 (WM_PRINT) ---- */
		struct PreviewItem
		{
			HWND hwnd = nullptr;
			HFONT hFont = nullptr; // per-preview HFONT (created with preview window DPI)
			CStringW cls;
			CStringW text;
			DWORD style = 0;
			DWORD exStyle = 0;
			CSize size{ 0,0 };
			bool inited = false;
		};

		HWND m_previewHostHwnd = nullptr;
		std::unordered_map<UINT, PreviewItem> m_preview;

        /*! ---- 工具栏渲染缓存 ---- */
		struct ToolbarRender
		{
			UINT id = 0;
			std::vector<HBITMAP> btnBmps; // per-button bitmap (owned)
			HBITMAP maskBmp = nullptr; // reusable white mask bitmap for disabled overlay
            /*! \brief 可选的隐藏 HWND，用于通过 WM_PRINT 实现像素级精确的按钮渲染（作为预览承载窗口的子窗口创建）。
             *  这些窗口由本类持有，清除渲染缓存时必须销毁。
             */
			std::vector<HWND> btnHwnds;
			int btnW = 16;
			int btnH = 16;
			// per-button source widths (when bitmap frames do not divide evenly)
			std::vector<int> btnWidths;
		};
		std::unordered_map<UINT, ToolbarRender> m_toolbarRenders; // key = toolbar resource id

        /*! \brief 在指定画布矩形上绘制工具栏资源 */
		void DrawToolbarCanvas(CDC& dc, const CRect& canvasRect, const SToolbarInfo& tb) const;
        /*! \brief 确保从 PE 资源加载工具栏位图到缓存（非常量方法） */
		bool EnsureToolbarRender(UINT toolbarId, const SToolbarInfo& tb);

        /*! \brief 当前激活的工具栏（设置后替代对话框显示） */
		std::shared_ptr<SToolbarInfo> m_activeToolbar;
		bool m_showingToolbar = false;
        /*! \brief 单个选中工具栏项的索引（便捷缓存） */
        int m_selectedToolbarIndex = -1;
        /*! \brief 鼠标按下时用于反馈的工具栏按钮索引（-1 表示无） */
        int m_toolbarPressedIndex = -1;
        /*! \brief 当工具栏处于激活状态时，当前选中的按钮索引集合（空表示无） */
        std::vector<int> m_selectedToolbarIndices;

        /*! \brief 工具栏选择访问器（供宿主/属性网格使用） */
    public:
        bool IsToolbarSelected() const { return m_showingToolbar && !m_selectedToolbarIndices.empty(); }
        // Return first selected index or -1
        int GetSelectedToolbarIndex() const { return m_selectedToolbarIndices.empty() ? -1 : m_selectedToolbarIndices.front(); }
        // Return all selected indices
        std::vector<int> GetSelectedToolbarIndices() const { return m_selectedToolbarIndices; }
        UINT GetSelectedToolbarItemId() const { int idx = GetSelectedToolbarIndex(); return (m_activeToolbar && idx >= 0 && (size_t)idx < m_activeToolbar->vecItemIDs.size()) ? m_activeToolbar->vecItemIDs[idx] : 0; }
        std::shared_ptr<SToolbarInfo> GetActiveToolbar() const { return m_activeToolbar; }
        // Set selected toolbar indices (replaces current selection). Notifies host.
        void SetSelectedToolbarIndices(const std::vector<int>& indices);
        /*! \brief 清除并释放所有已渲染的工具栏位图缓存 */
        void ClearToolbarRenders();
        //! 确保创建预览承载窗口（隐藏）
		void EnsurePreviewHost();
		//! 销毁预览承载窗口及其子预览控件
		void DestroyPreviewHost();
        //! 创建用于工具栏按钮渲染的隐藏 HWND（由本类持有）
		void CreateToolbarButtonHwnds(UINT toolbarId);
		//! 销毁单个预览控件
		void DestroyPreviewItem(PreviewItem& it);
		//! 确保创建/更新预览控件窗口
		bool EnsurePreviewHwnd(PreviewItem& it, DWORD exStyle, const CStringW& cls, const CStringW& text, DWORD style, const CSize& size);
		//! 按需初始化预览控件内部内容（Tab/Tree等）
		void InitPreviewContentIfNeeded(PreviewItem& it, const CStringW& cls);
        //! 将预览控件通过 WM_PRINT 绘制到指定 DC
        bool PrintPreviewToDc(CDC& dc, HWND hwnd, const CRect& targetCanvasRect) const;

        /*! ---- 模型 (Model) ---- */
		std::shared_ptr<SDialogInfo> m_dialog;

        /*! ---- 选择 (Selection) ---- */
		int m_selectedIndex = -1;
		std::vector<int> m_multiSelected;
		bool m_dialogSelected = false;
        /*! \brief Marquee（框选）选择状态 */
        bool m_marqueeSelecting = false;
        /*! \brief 可能的框选起点（在空白区域鼠标按下），拖动时激活 */
        bool m_marqueePossible = false;
        CPoint m_marqueeStartCanvasPt{}; // in canvas coords
        CPoint m_marqueeStartDlgPt{}; // in dialog-client coords
        /*! \brief 选择恢复点（在开始框选时保存） */
        int m_restoreSelectedIndex = -1;
        std::vector<int> m_restoreMultiSelected;

        /*! ---- 等级 / 徽章 (Level / badge) ---- */
		int m_currentLevel = 1;
		bool m_levelManagerConfigMode = false;

        /*! ---- 下拉组合框预览（类似 ResourceHacker） ---- */
		UINT m_activeComboId = 0; // 0 = none
		CRect m_activeComboDrawRectCanvas{}; // cached for dismiss/hit-test
		bool IsComboClass(const CStringW& cls) const;
		int GetComboCollapsedHeightPx(const CRect& droppedRect) const;
		CRect GetComboDroppedRectCanvas(const CRect& ctrlCanvasRect) const;
		CRect GetComboCollapsedRectCanvas(const CRect& droppedCanvasRect) const;
		CRect GetComboDropButtonRectCanvas(const CRect& collapsedCanvasRect) const;
		bool IsActiveCombo(UINT ctrlId) const { return m_activeComboId != 0 && m_activeComboId == ctrlId; }

        /*! ---- 度量 (Metrics) ---- */
		CSize m_baseUnits{ 0,0 };
        // Cached dialog HFONT (owned). If null, use stock DEFAULT_GUI_FONT.
        //! 缓存的对话框 HFONT（由本类持有）。若为空则使用系统默认 GUI 字体。
        HFONT m_hDialogFont = nullptr;

        //! 每个 DPI 下缓存的 HFONT（键为 LOGPIXELSY）
        std::unordered_map<int, HFONT> m_hDialogFontsPerDpi;

        /*! \brief 获取或创建指定 DPI 下的缓存 HFONT。返回存储在映射中的 HFONT（由本类持有）或 nullptr。*/
        HFONT GetCachedDialogFontForDPI(int logpixy);

        /*! \brief 根据 m_dialog->pFontInfo 更新缓存的 HFONT（按需创建/销毁） */
        void UpdateDialogFont();

        /*! \brief 从 SFontInfo 创建 HFONT（返回由调用方接管或 nullptr） */
        static HFONT CreateHfontFromSFontInfo(const SFontInfo* pfi);
        /*! \brief 在指定 DPI 下从 SFontInfo 创建 HFONT（由本类持有） */
        static HFONT CreateHfontFromSFontInfoWithDPI(const SFontInfo* pfi, int logpixy);

        /*! \brief 从给定 HFONT 计算对话框基础单位（若 hf==nullptr 则回退到系统单位） */
        static CSize ComputeBaseUnitsFromFont(HFONT hf);

        /*! ---- 滚动 (Scrolling) ---- */
		CPoint m_scroll{ 0,0 };
		CSize m_contentSize{ 0,0 };
		bool m_inUpdateScrollbars = false;

        /*! ---- 拖拽 (Drag) ---- */
		enum class DragMode
		{
			None,
			Move,
			SizeNW,
			SizeN,
			SizeNE,
			SizeE,
			SizeSE,
			SizeS,
			SizeSW,
			SizeW,
		};

		bool m_dragging = false;
		bool m_dragOnDialog = false;
		bool m_previewActive = false;
		bool m_dragOnComboDrop = false;
		CRect m_previewItemRect{};          // dialog client coords (px)
		CRect m_previewDialogRectCanvas{};  // canvas coords
		DragMode m_dragMode = DragMode::None;
		CPoint m_dragStartPt{};            // dialog client coords (px)
		CRect m_dragStartRect{};           // dialog client coords (px)
		CPoint m_dragStartPtCanvas{};
		CRect m_dragStartDialogRectCanvas{};

        /*! ---- 布局矩形 (Layout rects) ---- */
		CRect m_dialogRectCanvas{ 0,0,0,0 }; // window rect in canvas coords

        /*! ---- 帮助函数（从 CDesignSurfaceWnd 移植） ---- */
        //! 获取系统对话框 DLU 基准
		static CSize GetSystemDialogBaseUnits();
		//! DLU->像素（X轴）
		static int DluToPxX(int dluX, CSize baseUnits);
		//! DLU->像素（Y轴）
		static int DluToPxY(int dluY, CSize baseUnits);
		//! DLU 矩形->像素矩形
		static CRect DluToPxRect(const CRect& dluRect, CSize baseUnits);

		// 通知宿主窗口：选中/属性发生变化
		// 使用枚举替代纯数字以避免混淆，第二个参数用于携带可选数据（例如工具栏按钮的 itemId）
		enum class ESurfaceNotify : UINT
		{
			// 通用选择/刷新通知（无额外数据）
			SelectionChanged = 0,
			// 对话标题已修改（lParam 未使用）
			DialogCaptionChanged = 1,
			// 控件属性已修改（值已提交，lParam 未使用）
			CtrlPropertiesChanged = 2,
			// 对话大小/布局已修改（lParam 未使用）
			DialogSizeChanged = 3,
			// 工具栏视图已选择/切换（lParam 未使用）
			ToolbarSelectionChanged = 200,
			// 工具栏按钮被点击，lParam = itemId
			ToolbarButtonClicked = 100,
		};
		void NotifySelectionChanged(ESurfaceNotify code, LPARAM data = 0) const;
		// 兼容旧代码：接受 UINT 型的通知码并转换为枚举后转发
		void NotifySelectionChanged(UINT code, LPARAM data = 0) const { NotifySelectionChanged(static_cast<ESurfaceNotify>(code), data); }

		// 异步通知宿主（延迟/任务队列方式），用于在初始化/批量设置时避免同步回调导致的重入
		// 与 NotifySelectionChanged 不同，Deferred 版本会使用 PostMessage 发送消息，
		// 使宿主在当前调用栈返回后再处理通知。参考 RcCore 风格的注释（中文）。
		void NotifySelectionChangedDeferred(ESurfaceNotify code, LPARAM data = 0) const;

		//! 更新对话框窗口矩形（画布坐标）
		void UpdateDialogRect();
		//! 获取对话框窗口矩形（画布坐标，包含非客户区）
		CRect GetDialogRectCanvas() const;
		//! 获取对话框客户区矩形（画布坐标）
		CRect GetDialogClientRectCanvas() const;
		//! 获取对话框客户区原点（画布坐标）
		CPoint GetDialogClientOriginCanvas() const;
		//! 客户区坐标->画布坐标（考虑滚动偏移）
		CPoint ClientToCanvas(const CPoint& clientPt) const;
		//! 将画布坐标转换为对话框客户区坐标
		bool CanvasPointToDialogClient(const CPoint& canvasPt, CPoint& outDlgPt) const;
		//! 对话框客户区矩形（像素）->画布矩形
		CRect DialogRectToCanvas(const CRect& dlgRectPx) const;
		//! 获取窗口边框厚度（X）
		int GetBorderX() const;
		//! 获取窗口边框厚度（Y）
		int GetBorderY() const;
		//! 获取标题栏高度
		int GetCaptionHeight() const;

		//! 绘制对话框框架（含标题栏等）
		void DrawDialogFrame(CDC& dc) const;
		//! 更新滚动条（水平+垂直）
		void UpdateScrollbars();
		//! 更新水平滚动条
		void UpdateHScroll();
		//! 更新垂直滚动条
		void UpdateVScroll();

		int HitTestItem(const CPoint& canvasPt) const;
		int HitTestItemDlgClient(const CPoint& dlgPt) const;
		DragMode HitTestHandleOnly(const CPoint& canvasPt, const CRect& canvasRect) const;
		CRect HandleRectCanvas(DragMode mode, const CRect& canvasRect) const;
		DragMode HitTestDialogBorderCanvas(const CPoint& canvasPt) const;
		HCURSOR CursorForDragMode(DragMode mode) const;

		static CRect NormalizeRect(const CRect& r);
		CRect NormalizeAndClampItemRect(const CRect& dlgRectPx) const;
		CRect ApplyDragItem(const CRect& start, const CPoint& delta, DragMode mode) const;
		CRect ApplyDragDialogCanvas(const CRect& start, const CPoint& delta, DragMode mode) const;
		CPoint MaybeSnap(const CPoint& v) const;

		void DrawSelectionCanvas(CDC& dc, const CRect& canvasRect) const;
		void DrawPreviewRectCanvas(CDC& dc, const CRect& canvasRect) const;
		void DrawLevelBadgeOverlay(CDC& dc, const CRect& targetCanvasRect, int level, bool enabled) const;
		bool ShouldDrawLevelBadges() const;

        /*! ---- 网格与对齐参考线 ---- */
		struct Guides
		{
			bool hasV = false;
			int x = 0;
			bool hasH = false;
			int y = 0;
		};
		Guides m_guides;
		int m_gridStep = 8; // px
		bool m_showGrid = true;
		bool m_showGuides = true;
		void DrawGrid(CDC& dc, const CRect& dialogClientCanvas) const;
        /*! \brief 当 WM_PRINT 预览不可用时的备用控件渲染器。
         *  传入 style/exStyle 以便备用渲染能正确绘制 BS_CHECKBOX/BS_GROUPBOX/ES_MULTILINE 等类特定视觉效果。
         */
        void DrawFallbackControl(CDC& dc, const CRect& canvas, const CStringW& cls, const CStringW& text, DWORD style = 0, DWORD exStyle = 0) const;
		void DrawGuides(CDC& dc, const CRect& dialogClientCanvas) const;
		void UpdateGuidesForMove(const CRect& movingDlgPx);

		// ---- 动态窗口布局（为缺少位置信息的动态窗口生成默认布局） ----
		// 使用 VS RC 编辑器常见的默认控件大小，按行从左到右排列（每行最多 10 个），
		// 高个控件（list/tree 等）放置在后排，保证动态窗口在设计面板上可见且可交互。
		void PrepareDynamicDialogLayout();
		CSize GetDefaultDynamicCtrlSizeDlu(const CStringW& cls, bool& outTall) const;
	};
}


/* ===================== 原文件：FormRcEditor.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]

#include <afxwin.h>
#include <afxcmn.h>
#include <commctrl.h>
#include <map>
// [已合并依赖] #include "CDesignSurfaceWnd.h"
// [已合并依赖] #include "RcEditorModel.h"
// [已合并依赖] #include "CPropertyListCtrl.h"


// CFormRcBase.h 已由上方拆分后的 UI 头文件提供。


/*!
 * \brief RC 编辑器主窗口基类，支持多资源文件、等级管理、属性编辑等功能
 * \details 提供树控件、设计面板、属性网格等核心 UI 及相关操作钩子
 */
class CFormRcEditor : public ParamRange::CFormNoRcBase
{
    DECLARE_DYNAMIC(CFormRcEditor)

public:
    /*!
     * \brief 构造函数
     * \param pParent 父窗口指针，可为 nullptr
     */
    explicit CFormRcEditor(CWnd* pParent = nullptr);
    /*! \brief 析构函数 */
    virtual ~CFormRcEditor();

protected:
    /*!
     * \brief 初始化对话框（MFC 钩子）
     */
    virtual BOOL OnInitDialog() override;

    /*!
     * \brief 获取窗口初始标题
     */
    virtual CStringW GetInitialCaption() const { return L"RC Editor"; }

    /*!
     * \brief 控件创建后初始化 UI 和菜单（派生类可重写）
     */
    virtual void OnAfterControlsCreated() {}

    // 内部等级索引不变，允许派生编辑器采用用户界面的等级编号。
    virtual int GetLevelDisplayNumber(int levelIndex) const { return levelIndex + 1; }

    /*!
     * \brief 对话框关闭前的处理（派生类可重写）
     */
    virtual void OnBeforeDialogClose() {}

    // ---- 派生窗口可重写的钩子 ----
    /*!
     * \brief 启用 / 禁用等级管理器配置模式
     * \param enable 是否启用
     */
    void SetDesignSurfaceLevelConfigMode(bool enable);

    /*!
     * \brief 重新创建树控件以启用 / 禁用复选框
     * \param enable 是否启用复选框
     */
    void SetTreeCheckboxesEnabled(bool enable);

    /*!
     * \brief 设置窗口标题
     * \param caption 标题内容
     */
    virtual void SetWindowCaption(const CStringW& caption);
    
    // ---- 供派生窗口使用的编辑器模型视图 ----
    /*!
     * \brief 获取当前可编辑的资源信息
     * \return 指向当前可编辑资源信息的共享指针（可能为 nullptr）
     */
    std::shared_ptr<rceditor::SSingleResourceInfo> GetEditableResource() const { return m_pRes; }

    /*!
     * \brief 获取当前等级（1-8）
     * \return 当前选中的等级
     */
    int GetCurrentLevel() const { return m_curLevel; }

public:
    /*!
     * \brief 获取已打开资源的引用
     * \return 已打开资源的 const 引用（键为文件路径）
     */
    const std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>& GetOpenedResources() const { return m_openedResources; }

protected:

    // 等级界面钩子（派生类可覆盖）
    /*!
     * \brief 创建等级管理器相关 UI 控件（复选框、等级下拉框等）
     */
    virtual void CreateLevelUiControls();
    /*!
     * \brief 切换等级管理器UI控件的可见性
     * \param visible 是否显示
     */
    virtual void OnLevelUiVisibilityChanged(bool visible);
    /*!
     * \brief 布局等级管理器相关UI控件
     * \param rcCheck 全选复选框区域
     * \param rcCombo 等级下拉框区域
     */
    virtual void LayoutLevelUiControls(const CRect& rcCheck, const CRect& rcCombo);
    /*!
     * \brief 处理全选复选框点击事件（派生类可重写）
     */
    virtual void HandleCheckAllClicked();
    /*!
     * \brief 处理等级下拉框选择变化事件（派生类可重写）
     */
    virtual void HandleBadgeSelChange();
    /*!
     * \brief 窗口大小变化事件
     */
    afx_msg void OnSize(UINT nType, int cx, int cy);

    /*!
     * \brief 擦除背景事件
     */
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    /*!
     * \brief 销毁窗口事件
     */
    afx_msg void OnDestroy();

    /*!
     * \brief 获取窗口最小/最大尺寸事件
     */
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    /*!
     * \brief 属性网格属性变更处理
     */
    afx_msg LRESULT OnPropertyChanged(WPARAM wParam, LPARAM lParam);
    /*!
     * \brief 树控件选择变化事件
     */
    afx_msg void OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 树控件项变化事件
     */
    afx_msg void OnTvnItemchangedTree(NMHDR* pNMHDR, LRESULT* pResult);

    /*!
     * \brief 树控件点击事件
     */
    afx_msg void OnNMClickTree(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

protected:
	struct TreeNodeData
	{
        enum { ToolbarCategoryId = 0xF002 }; // 区分工具栏分类与其他分类
		// 节点种类，便于扩展不同资源类型（文件根、分类、对话框、控件、菜单、工具栏等）
        enum NodeKind { NK_FileRoot = 0, NK_Category = 1, NK_Dialog = 2, NK_Control = 3, NK_Menu = 4, NK_Toolbar = 5, NK_ToolbarItem = 6, NK_DynamicWnd = 7, NK_DynamicItem = 8, NK_Other = 100 } kind;
		// 对于 Dialog 节点：dlgId 保存对话框 ID；对于 Menu/Toolbar 节点：dlgId 保存对应资源 ID。
		UINT dlgId;
		// 对于 Control 节点：ctrlId 保存控件 ID；对于非控件节点使用 UINT_MAX。
		UINT ctrlId; // UINT_MAX => file root / category / non-control
		CString filePath; 	//! 所属文件的key
	};

	enum ECtrlsID
	{
		IDC_TOOLBAR_RCED = 5000,
		IDC_TREE_RCED = 5001,
		IDC_SURFACE_RCED = 5002,
		IDC_PROPGRID_RCED = 5003,
		IDC_EDIT_SEARCH_RCED = 5004,
		IDC_BTN_SEARCH_RCED = 5005,
	IDC_CHECK_ALL_RCED = 5006,
		IDC_COMBO_BADGE_RCED = 5007,
		IDC_PROPLIST_RCED = 5008,  // 属性列表控件ID（用于替代/补充属性网格）
		IDC_MENU_BASE_RCED = 6000,
	};

	enum ECmdID
	{
		ID_RCED_FILE_OPEN = IDC_MENU_BASE_RCED + 1,
		ID_RCED_FILE_SAVE = IDC_MENU_BASE_RCED + 2,
		ID_RCED_FILE_SAVE_AS = IDC_MENU_BASE_RCED + 3,
		ID_RCED_FILE_EXIT = IDC_MENU_BASE_RCED + 4,
	};

protected:

    // 核心 UI 资源，供派生类使用
    CFont m_font;
    CToolBarCtrl m_toolbar; // 工具栏控件
    CTreeCtrl m_tree; // 树视图
    CEdit m_editSearch;
    ParameterEditorStyle::CButton m_btnSearch;
    // 等级管理器 UI 使用的控件
    CButton m_checkAll;
    CComboBox m_comboBadge;
    // 设计面板和属性列表
    rceditor::CRcDesignSurfaceWnd* m_surface = nullptr;
    CPropertyListCtrl m_propList;  // 属性列表控件

    // 菜单对象
    CMenu m_menu;
    CMenu m_menuFile;
    CMenu m_menuEdit;

    CString m_currentIniPath;
    bool m_initialized = false;
    bool m_inPropUpdate = false;
    bool m_inTreeUpdate = false;
    bool m_inBadgeUpdate = false;
    bool m_closing = false;
    int m_curLevel = 1;
    // When true, UpdateBadgeUiFromSelection will not change the check state of m_checkAll.
    // Used to preserve the Select-All visual state during property-list driven updates.
    bool m_preserveBadgeCheck = false;
    CString m_searchText;
    HTREEITEM m_lastSelectedTreeItem = nullptr;

    // 为 true 时：树显示复选框，主 UI 显示用于等级管理的复选框/下拉选择
    bool m_showLevelUi = false;
    
    // 搜索模式标志：为 true 时表示当前显示的是搜索结果树，而非原型树
    // 仅通过搜索/清除按钮切换，其它操作（如勾选复选框、全选等）不会改变此标志
    bool m_isSearchMode = false;

    // When in search mode, store matched control indices for the currently selected dialog
    // to support keyboard navigation (up/left, down/right) between matches.
    std::vector<int> m_searchMatchedIndices;
    UINT m_searchMatchedDlgId = 0;

    std::shared_ptr<rceditor::PEFileEditor> m_pe;
    std::shared_ptr<rceditor::SSingleResourceInfo> m_pRes;
    // 当以类似工具栏的预览显示动态窗口时，记录其对话框模型
    std::shared_ptr<rceditor::SDialogInfo> m_activeDynamicDialog;
    CString m_activeDynamicFilePath;
    std::vector<std::unique_ptr<TreeNodeData>> m_treeNodeData; // 为 CTreeCtrl 保持指针稳定


	//! 支持多文件打开 - CString 为文件路径
    std::map<CString, std::shared_ptr<rceditor::PEFileEditor>> m_openedEditors;
    std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>> m_openedResources;

protected:
	bool OpenFile(const CString& path);        // 打开（或复用）并设为活动文件
	void SetActiveFile(const CString& path);   // 切换活动文件（更新 UI / surface / tree）
	void CloseFile(const CString& path);       // 可选：关闭并移除资源
	CString GetActiveFilePath() const { return m_currentIniPath; }

    // 向派生类暴露一些布局和更新帮助函数（由 CFormUIDesigner 提供）
    void CreateControls(); // 创建 UI 控件
    virtual void LayoutControls(); // 布局控件
    virtual bool UseParameterEditorStyle() const { return false; }

    void CreateMenus(); // 创建菜单
    void CreateToolbar(); // 创建工具栏
    void UpdateToolbarLayout(); // 更新工具栏布局（占位）

    // 主 Tab 放在文档设定分组下，子窗口挂到所属 Tab；搜索时按需补齐父节点。
    HTREEITEM GetLevelAuditGroup(HTREEITEM root, const CString& filePath);
    CString GetLevelAuditDialogText(const CString& filePath, const rceditor::SDialogInfo& dlg) const;
    HTREEITEM InsertDialogTreeItem(HTREEITEM root, HTREEITEM defaultParent,
        const CString& filePath, const rceditor::SDialogInfo& dlg);
    HTREEITEM GetLevelAuditParent(HTREEITEM root, HTREEITEM defaultParent,
        const CString& filePath, const rceditor::SDialogInfo& dlg);
    HTREEITEM FindTreeItemForNode(int kind, UINT id, const CString& filePath = CString()) const;
    virtual void RebuildTree(); // 重建原型树结构（等级管理器覆盖业务分类）
    virtual void SyncLevelTreeParentChecks() {} // 在模型勾选同步后更新业务父节点
    virtual void RebuildSearchTree(); // 重建搜索结果树（派生类可覆盖以实现自定义搜索逻辑）
    void RefreshCurrentTree(); // 根据当前模式刷新树（搜索模式刷新搜索树，否则刷新原型树）
    void UpdatePropertyGrid(); // 更新属性面板
    virtual void UpdatePropertyList(); // 更新属性列表控件
    virtual void OnPropertyListChanged(int nIndex, const CPropertyListCtrl::SPropertyItem& item); // 属性列表变更回调
    virtual void UpdateBadgeUiFromSelection(); // 根据选择更新 Badge UI
    // 为搜索扩展提供钩子：向临时树（root下）追加菜单/工具栏匹配项
    bool AppendSearchMatchesForResource(const std::shared_ptr<rceditor::SSingleResourceInfo>& res,
        const CString& filePath, const CString& searchLower, HTREEITEM hRoot);
    int CalcComboBestWidth(CComboBox& combo) const; // 计算 combo 最佳宽度
    int CalcComboDroppedHeight(CComboBox& combo, int visibleItems) const; // 计算下拉高度
    void UpdateBadgeComboMetrics(); // 更新 badge combo 的尺寸/下拉高度

private:
    // （重建树时不保存视觉状态；保持逻辑简单）

    // 重建多选的属性网格（显示共享属性，例如等级掩码）
    void RebuildPropertyGridForSelection();
    HTREEITEM FindTreeItemForDialog(UINT dlgId, const CString& filePath = CString()) const;
	HTREEITEM FindTreeItemForControl(UINT dlgId, UINT ctrlId, const CString& filePath = CString()) const;
	void SyncTreeSelectionFromSurface();

    // 暴露给实现层的工具函数
    static bool FuzzyMatchNoCase(const CString& haystack, const CString& needle);

    bool PickOpenFile(CString& outPath) const;
    bool PickSaveFile(CString& outPath) const;

    static bool TryParseDword(const CString& s, DWORD& outValue);

private:
    // 命令处理函数
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	afx_msg void OnFileExit();

	afx_msg void OnSearchChanged();
	virtual void OnSearchTextChanged();
	afx_msg void OnSearchClicked();
	afx_msg void OnCheckAllClicked();
	afx_msg void OnBadgeSelChange();
	afx_msg LRESULT OnDesignSurfaceNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPostSyncTreeChecks(WPARAM wParam, LPARAM lParam);
};


/* ===================== 原文件：FormLevelManagerEditor.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]

// [已合并依赖] #include "FormRcEditor.h"

/*!\
 * \brief 等级管理器编辑器窗口类，继承自 CFormRcEditor\
 * \details 提供等级 JSON 配置的加载、保存、UI 操作等功能\
 */
class CFormLevelManagerEditor : public CFormRcEditor
{
    DECLARE_DYNAMIC(CFormLevelManagerEditor)

public:
    /*!
     * \brief 构造函数
     * \param pParent 父窗口指针，可为 nullptr
     */
    explicit CFormLevelManagerEditor(CWnd* pParent = nullptr);
    /*! \brief 析构函数 */
    virtual ~CFormLevelManagerEditor();

    /*!
     * \brief 设置初始文件列表（PE资源和JSON配置）
     * \param binPaths 需要打开的PE文件路径列表
     * \param jsonPath 可选JSON配置文件路径
     */
    void SetInitialFiles(const std::vector<CString>& binPaths, const CString& jsonPath = CString());

    /*!
     * \brief 设置已打开资源的共享指针（直接共享外部 m_openedResources）
     * \param pOpenedResources 指向已打开资源映射的指针
     * \note 如果设置了此指针，将直接使用外部资源，而不是通过 OpenFile 加载
     */
    void SetOpenedResources(std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>* pOpenedResources,
        const CString& loadedJsonPath = CString());
    const CString& GetLoadedJsonPath() const { return m_loadedJsonPath; }

    // 获取当前（可能已修改的）打开资源映射，供外部同步
    const std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>& GetOpenedResources() const { return m_openedResources; }

protected:
    /*! \brief 获取窗口初始标题 */
    CStringW GetInitialCaption() const override;
    void SetWindowCaption(const CStringW&) override { CFormRcEditor::SetWindowCaption(GetInitialCaption()); }
    /*! \brief 控件创建后初始化等级管理器相关UI和菜单 */
    void OnAfterControlsCreated() override;
    int GetLevelDisplayNumber(int levelIndex) const override { return levelIndex; }
    bool UseParameterEditorStyle() const override { return true; }
    bool ShowParameterCloseButton() const override { return false; }
    void LayoutControls() override;
    /*! \brief 对话框关闭前的处理（如保存JSON、关闭等级管理模式） */
    void OnBeforeDialogClose() override;

    // Level UI overrides
    /*! \brief 创建等级管理器相关的UI控件（全选复选框、等级下拉框） */
    void CreateLevelUiControls() override;
    /*!
     * \brief 切换等级管理器UI控件的可见性
     * \param visible 是否显示
     */
    void OnLevelUiVisibilityChanged(bool visible) override;
    /*!
     * \brief 布局等级管理器相关UI控件
     * \param rcCheck 全选复选框区域
     * \param rcCombo 等级下拉框区域
     */
    void LayoutLevelUiControls(const CRect& rcCheck, const CRect& rcCombo) override;
    /*! \brief 处理全选复选框点击事件 */
    void HandleCheckAllClicked() override;
    /*! \brief 处理等级下拉框选择变化事件 */
    void HandleBadgeSelChange() override;

    /*! \brief 重建搜索结果树（覆盖基类实现，提供自定义搜索逻辑） */
    void RebuildTree() override;
    void RebuildSearchTree() override;
    void SyncLevelTreeParentChecks() override;
    bool GetDialogTreeCheckTargets(const TreeNodeData& data,
        std::vector<std::shared_ptr<rceditor::SDialogInfo>>& targets) const;
    bool SetDialogTreeChecked(const TreeNodeData& data, bool checked);
    bool ToggleDialogTreeChecked(HTREEITEM item);
    bool GetToolbarTreeCheckTargets(const TreeNodeData& data,
        std::vector<std::shared_ptr<rceditor::SToolbarInfo>>& targets) const;
    bool SetToolbarTreeChecked(const TreeNodeData& data, bool checked);
    bool ToggleToolbarTreeChecked(HTREEITEM item);
    void NormalizeExemptToolbarItems();

    /*!
     * \brief 属性列表变更处理，标记数据为已修改
     * \param nIndex 变更的属性索引
     * \param item 属性数据
     */
    void OnPropertyListChanged(int nIndex, const CPropertyListCtrl::SPropertyItem& item);

    /*!
     * \brief 属性网格属性变更处理，仅允许等级相关属性修改
     * \param wParam 未使用
     * \param lParam 属性指针
     * \return 处理结果
     */
    LRESULT OnPropertyChanged(WPARAM wParam, LPARAM lParam);
    /*!
     * \brief 树控件点击事件处理，标记数据为已修改
     * \param pNMHDR 通知消息头
     * \param pResult 结果指针
     */
    void OnNMClickTree(NMHDR* pNMHDR, LRESULT* pResult);
    /*!
     * \brief 树控件选择变化事件处理，支持搜索结果控件的自动选中
     * \param pNMHDR 通知消息头
     * \param pResult 结果指针
     */
    afx_msg void OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);
    /*! \brief 树控件项状态变化处理（文档树联动及非管理员勾选恢复） */
    afx_msg void OnTvnItemchangedTree(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

    // Keyboard handling for navigating matched controls in search mode
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

private:
    ParameterEditorStyle::CButton m_openResourceButton;
    ParameterEditorStyle::CButton m_openJsonButton;
    ParameterEditorStyle::CButton m_saveJsonButton;
    ParameterEditorStyle::CButton m_saveAsButton;
    ParameterEditorStyle::CButton m_closeButton;
    ParameterEditorStyle::CHeader m_styledPropertyHeader;
    CStatic m_levelLabel;
    CStatic m_fileStatus;
    CString m_jsonPath;
    CString m_loadedJsonPath;
    bool m_savedToTarget = false;
    void RefreshFileDisplay();
    bool m_dirty = false; // 数据是否已修改
    enum { ID_LM_OPEN_JSON = 41000, ID_LM_SAVE_JSON = 41001 };

    /*!
     * \brief 弹出文件选择对话框，选择要打开的JSON文件
     * \param outPath 输出选中的文件路径
     * \return 是否成功选择文件
     */
    bool PickJsonOpenFile(CString& outPath) const;
    /*!
     * \brief 弹出文件保存对话框，选择保存JSON的路径
     * \param outPath 输出选中的保存路径
     * \return 是否成功选择路径
     */
    bool PickJsonSaveFile(CString& outPath) const;

    /*!
     * \brief 加载等级管理器JSON配置
     * \param jsonPath JSON文件路径
     * \return 加载是否成功
     */
    bool LoadLevelConfigJson(const CString& jsonPath);
    /*!
     * \brief 保存当前等级管理器配置到JSON文件
     * \param jsonPath 目标JSON文件路径
     * \return 保存是否成功
     */
    bool SaveLevelConfigJson(const CString& jsonPath);
    /*! \brief 标记当前数据为已修改（脏） */
    void MarkDirty();
    /*!
     * \brief 如果有未保存的修改则保存JSON
     * \param forcePickPath 是否强制弹出保存路径选择
     * \return 保存是否成功
     */
    bool SaveIfDirty(bool forcePickPath);

    /*! \brief 打开等级管理器JSON文件 */
    afx_msg void OnFileOpenLevelJson();
    /*! \brief 另存为等级管理器JSON文件 */
    afx_msg void OnFileSaveAsLevelJson();
    /*! \brief 保存等级管理器JSON文件（如有路径则直接保存） */
    afx_msg void OnFileSaveJson();
};


/*
==================================================================================
                                四、等级数据与旧版总对话框
==================================================================================
*/

/* ===================== 原文件：paralevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
/*
���Ӳ��裺
step1:���Ӳ�����
step2:���Ӳ���L_COUNT��1
step3:��Ӧҳ��LEN��Ҫ��1
step4:����Ӷ�Ӧ�ַ�����ע��Ҳ��Ӧ����
*/
class AFX_EXT_CLASS CParaLevel
{
public:
	CParaLevel(void);
	~CParaLevel(void);

	BOOL m_bIsView;
		 //step1:���Ӳ�����
	enum {
		//ȱ�ݴ�С
		L_ETCH, L_EDGE, L_EDGE_TOL, L_EDGE_TOLC, L_SMD, L_SMD_PERCENT, L_SMD_JY, L_RING, L_HOLE, L_BIGSMD, L_BIGSMD_JY, L_MCOPPER, L_MCOPPER_JY, L_MCOPPER_OFFSET,
		L_MHOLE, L_MASK, L_MASK_JY, L_MASK_HOLE, L_LUTONG, L_SILK, L_BIGSILK, L_LINE, L_STEEL, L_RF4, L_PI, L_VOID, L_BLIND, L_BACKDRL, L_MASK_BRIDGE,L_MASK_BROKEN,
		L_MASK_COPPER_BLOCK, L_MASK_MAT_BLOCK, L_SMD_JY_DARK, L_BIGSMD_JY_DARK,L_MCOPPER_LIGHT_JY, L_MASK_LIGHT_JY,
		//��Ե���
		L_DEIN_HOLE, L_DEIN_SILK, L_DEIN_PAD, L_DEFAULT,
		//��Ե���->����
		L_MASK_RING_WIDTH, L_MASK_COPPER_DEFLATE, L_MASK_DEFLATE, L_MASK_MAT_TOL, L_VOID_DEFLATE, L_BACK_DRL, L_OUTLINE_DEFLATE, L_VOID_TEST_METHOD, L_BIG_COPPER_TOL,
		L_MCOPPER_EDGE, L_BOARD_EDGE_TOL, L_NPTH_HOLE, L_VOID_LUTONG, L_FINGER_SLOPE_TOL,
		//΢��λ
		L_ADJUST_SIZE, L_MASK_DEVIATE, L_ADJUST_HN, L_ADJUST_MC, L_ADJUST_BEDGE, L_SILK_OFFSET, L_SILK_ONLY, L_SILK_QUALITY, L_PADNUM,
		//΢��λ->����
		L_ADJUST_CPEDGE, L_MMAT_MAT_EDGE, L_REGION_ADJ, L_FINGER_CENTER, L_LIGHTPAD, L_TINBOARD,L_RINGANPAD_ADJUST,
		//����
		L_BLIND_TO_PAD, L_CHECK_JY_NEW, L_PAD_PINE_HOLE, L_SMD_JY_SIZE, L_BGA_POS, L_PAD_SIZE, L_SILK_TO_PAD,L_ARUOUNDMCPADSIZE,//��ӱ�޸�
		//����->����
		L_SMALL_SMD, L_SMALL_SMDA, L_THIN_SMD, L_CHECK_MASK_CIRCLE_SMD, L_TEST_JOINPAD, L_BIGCOP_EDGE, L_BUMP, L_PAD_OSM, L_GDSMD, L_PAD_EDGEOUT, L_LOST_GOLD,
		L_PAD_CONTOUR, L_NEARPAD_BUBBLE, L_PAD_DOPANT, L_SMD_HOTSPOT, L_THIN_SMD_EDGE, L_SMD_PAD_BUMP, L_SMD_BIGCOPPER_ERRSIZE,
		//�׺Ϳ׻�
		L_NHOLE_SIZE, L_MRING, L_CHECK_RING, L_CIRCLE_EDGE, L_MIN_RING, L_BLIND_SIZE, L_BLIND_E, L_BLIND_CR, L_SMD_BLIND,
		//�׺Ϳ׻�->����
		L_NPTH, L_LEVELINHOLE, L_MHOLE2, L_MASK_HOLE2, L_MHOLE_SIZE, L_GRIDE, L_RING_OSM, L_RHOLE_ADJUST, L_RING_LOST, L_HOLE_WALL, L_NOCHECK_HOLE_WALL, L_ADD_BIGHOLE_WALL, L_HOLE_JY, L_RING_LITTLE_TEST, 
		L_VOIDHOLE,
		//��·
		L_CHECK_LINE, L_CHECK_LINESP, L_CHECK_NOSPACE, L_CHECK_MAT_BRIDGE, L_CHECK_LINE_CORNER, L_LINE_SPACE_CORNER, L_CHECK_COPPER_LINE, L_LINE_WIDTH_RANGE,
		L_MIN_LINE_LENGTH, L_LINE_EDGE_THR, L_CPLINE_EDGE_THR, L_LINE_DEBRIS_EDGE_THR, L_DEBRIS_EDGE_THR, L_FINGER_DEBRIS_EDGE_THRESHOLD, L_CPLINER_DEBRIS_EDGE_THRESHOLD,
		L_CHECK_NEARPAD_LINE, L_LINE_BREAK, L_CHECK_BRIDGE, L_LINE_SPACE_FALSE_ERR, L_CHECK_LINE_WIDTH, L_CHECK_CURVE_LINE, L_COPPER_OUTLINE, L_LINE_EDGE_OUT, L_LINE_EDGE_IN,
		L_MCOPPER_OUTLINE_OUT, L_MASKHOLE_EDGE, L_VOID_EDGE, L_MIX_BRIDGE_COLOR, L_CHECK_MIX_BRIDGE_LINE,L_CHECK_EDIT_LINE_DILATION,
		//����
		L_CHECK_TIEBAR, L_SOFTFINGEROFF, L_PADMEASURE, L_CHECK_EXPAND, L_CHECK_PAD_SPACE, L_LOST_MASK, L_HOLE_SPACE_ERROR, L_CHECK_SILK_OFF, L_CHECK_MARK,
		//����
		L_VOID_OFFSET_TOL, L_MASK_OFFSET, L_SILK_OFFSETO, L_MASK_BRIGHTER, L_FINGER_SHORT, L_DUST_FILTER, L_FAT_SILK, L_TEXT_PAD_HOLE, L_MASK_ON_PAD, L_PAD_LUTONG,
		L_MAT_LUTONG, L_HOLE_NTR, L_VCUT, L_NG_ERR_NUM, L_SCAN_COL_COUNT, L_TIME_THRED, L_MCOPPER_IN_SILK, L_SURFACE_TYPE, L_MASK_COLOR, L_ERR_LEVEL,
		L_AUTO_PARA, L_LOAD_PARA_FILE, L_SAVE_PARA_FILE, L_CHECKSILKLOST, L_CHECK_SMDMATEDGE, L_CHECKMASKBYCV, L_SILK_PERCENTSIZE, L_TEST_TOPORBOT, L_CHECKEDGEKBYCV,
		//һ��->�˶��趨
		L_TIDY_OFFSET, L_PCB_THICKNESS, L_BPOS_TRIM, L_PCB_WARP, L_TWIST, L_BOW, L_RADIO_HOW,
		//һ��->��ά���趨
		L_SAVE_PASSIMG, L_SAVE_FAILIMG, L_ISSQUARE, L_CODE_TYPE, L_SAME_DECODES, L_TIMEOUTSEC, L_GRIDNUM, L_DCODE_BREAK, L_DETECT_LEN, L_DEDGE_DIFF,
		L_DMIN_RATIO,
		//һ��->AI�ر�ȱ����������
		L_AISPECIAL_SETTING,
		//�������
		L_ADJUST_RFS, L_CHECK_STEEL, L_CHECK_SLOD, L_CHECK_COVER, L_RF_POS_TOL, L_LIGHT_VALUE, L_USE_DEFLATE_PARA,
		//��ײ���
		L_HOLE_INTERVAL, L_HOLE_L1, L_HOLE_L2, L_HOLE_L3, L_HOLE_L4, L_HOLE_L5, L_HOLE_ERR_SIZE, L_HOLE_DRL_MORE_LESS, L_HOLE_VOID_SIZE, 
		L_HOLE_VOID, L_BOARD_OUTLINE, L_VOID_EDGE_TOL,
		//��ײ���->�߼�
		L_PTH1, L_PTH2, L_PTH3, L_PTH4, L_PTH5, L_NPTH1, L_NPTH2, L_NPTH3, L_NPTH4, L_NPTH5, L_VOIDHOLE_TOL,

		//��ɫ����
		L_COLOR, L_COLORBZ, L_COLORJY, L_COPYPARA, L_LUMINANCE, L_LIGTTYPE, L_COPY, L_KUSELECT, L_KUSAVE,
		//�û�����->һ��
		L_JOB_PATH, L_REPORT_PATH, L_LOG_PATH, L_FM_VRS, L_AI_JPG, L_FM_JOB_PATH, L_DECODE_SAVE, L_DECODE_PATH, L_SCAN_MACHINE,
		//�û�����->����
		L_WND_NUM, L_ERRDIB_SIZE, L_MAXERR_NUM, L_ERRWND_RECT, L_ERR_SHAPE, L_SHOW_WARPING, L_ERR_POS, L_SPECIFY_PCBNO, L_SPECIAL_ERR, L_NG_OKMODE, L_NG_IGNORE, L_BOARD_SPACE,
		L_SAVESAME_DECODES, L_SET_ERRPIXEL, L_PL_SIZE, L_PL_COUNT, L_MACHINE_NO, L_DEVICE_CODE, L_AUTO_PRINT, L_SHOW_ONESIDE, L_VOID_PASS,
		L_MIN_VOID, L_NG_ASPASS, L_PASS_ASFAIL, L_DECODE_NG_AS_FAIL, L_DECODE_SMAE_AS_NG, L_DC_MOVE_TO, L_DECODE_NTR, L_SAME_DECODE, L_DECODE_TIP_NUM, L_COMBINE_ERR,
		L_NTR_FILTER_REMIND, L_USERINPUT_DECODE, L_SAME_DECODE_STOP, L_NG_NOSAVE_DECODE, L_TEST_SIGNCODE_AND_DECODE,L_LOCK_USER_PARA, L_COMBINE_CHAR_POS, L_IF_CHARS_DIFF,
		L_UNITNOCHECK,
		//�û�����->����->Debug��ͼ����
		L_SAVEPREIMAGE, L_SAVEIMAGE, L_NGSAVEIMAGE, L_TIMEOUTSAVEIMAGE, L_ERRCNTSAVEIMAGE, L_ERRCNTTHR,
		//�߼��趨
		L_MACHINE_TYPE, L_PRECISION, L_BOARD_ORG_X, L_BOARD_ORG_XB, L_BOARD_WIDTH, L_BOARD_WIDTH_B, L_CCD_TYPE, L_USELOCALIP, L_TOP_EDGE, 
		L_LAN, L_TOPSIDE_ONLY, L_PARAMETER_RANGE, L_DO_POINT, L_PARAM_CANCLE, L_AUTO_DOT, L_RAM_DISK, L_CCD_ZOOM, L_MAKEUP_ONE_CODE, 
		L_CHECK_BMP, L_PASS_SORT, L_CHECK_BLIND, L_CHECK_EAP, L_EAP_SETTING, L_CHECK_UPTODOWN, L_SORT_BY_OUTLINE, L_USE_AI, L_SHOW_AI_ONLY, 
		L_AI_IP, L_AI_MAX, L_OFFSET_CCD, L_SERVER_IP, L_STDBOARD, L_HIGHSPEECHMODE, L_HOLE_CCD, L_CAR_SYSTEM, L_PASSWORD, L_MOVE_SETTING, L_TRYTEST_BOTH,L_LOAD_PART_GERBER,
		L_NOTEST_HOLE, L_NOTEST_IMG,
		//����
		L_VIEW_LEVEL, L_VIEW_FILTER, L_VIEW_NTR, L_VIEW_MASK_COPPER, L_VIEW_MASK_MAT, L_VIEW_MAT, L_VIEW_SILK, L_VIEW_PAD, L_VIEW_EDIT, L_AI_KEY_AREA,

		//���޲���->��������
		L_VPATH_JOB, L_VPATH_REPORT, L_VPATH_LOG, L_VDECODE_SAVE, L_VDECODE_PATH,
		//���޲���->�豸�趨
		L_VMACHINE_NO, L_VDEVICE_CODE, L_VLIST_MACHINE, L_VMOVE_BY_RIGHT, L_VCCD_ZOOM, L_VWITH_MACHINE, L_VDCMOVE_TO, L_VCCD_PRECISION, L_VDECODEINFO_SET,
		L_VHAVE_LASER, L_VHAVE_SCAN, L_VSCAND_DECODE, L_VALL_PIECES, L_VONLY_SCANPL, L_VHAVE_PRO, L_VPROJECTOR, L_VLASER_SETTING, L_SCAN_BAR_CODE, L_CODE_READER,
		//���޲���->ȱ�ݴ�������
		L_ERRWND_HORZNUM, L_ERRWND_VERTNUM, L_VPOS_MODE, L_VLINE_WIDTH, L_VPRECISION, L_ERR_BK_GD, L_BACKGROUND, L_ALL_BRIGHT, L_BOTH_SHOW, 
		L_BRIGHT_SETTING, L_USE_RULER, L_USE_RULERU, L_RULER_STEP, L_SHOW_TEXT_STEP, L_ERR_WITH_GERBER, L_VUP_DOWN, L_IMAGE_UP, L_VMIDDLE, 
		L_VRIGHT, L_VBOTH_SIDE, L_VUSE_GERBER, L_BROW_WITHMACHINE, L_UNIT_POINT, L_ZOOM_GERBER, L_ONLEFT, L_INGROUP, L_PCB_LEFT, L_GERBERLEFT, 
		L_HORZ_PERCENT, L_VIEW_OK_PIC, L_VZOOM_IMAGE, L_JOB_RECT, L_IMAGE_SERVER_GERBER_COLOR_SET, L_SHOW_SETTING,
		//���޲���->ƫ��
		L_ROW_NUM, L_COL_NUM, L_UNIT_SORT, L_RECORD_SORT, L_EXPANDE_USER, L_LOCK_LIST, L_SHOW_BOTH_SIDE, L_ROTATE, L_ONLY_SHOW_TRUE, 
		L_SORT_BY_GERBER, L_ZSORT, L_DNTSORT, L_COL_FIRST, L_NGPCS, L_CONFIRM_SETTING, L_SET_HOTKEY, L_PRIOR_ERR, L_CODE_INSPECT, L_CODE_COMPARE, 
		L_FIRST_TIME, L_NEXT_ERR_PAGE_TIME, L_NEXT_ERR_TIME, L_SHAPE_SIZE, L_DONT_BROW,	L_BROW_SCRAP, L_Brow_Set, L_Brow_T, L_Brow_B, 
		L_Brow_TB, L_ONLINE_CONFIRM, L_ONLINE_CONFIRMB, L_ONLINE_CONFIRMA, L_NOCHANGE_NUM, L_LOCK_PAGE_KEY, L_HIDE_END, L_END_TRUE, L_SOFT_BOARD, 
		L_CANCEL_SIDETIP, L_READ_FAIL, L_LOCK_SIDE, L_SHOW_NG_ERR, L_CANCEL_VIEWED, L_SAMECODE_BOX, L_SORT_BY_BLIND, L_SIGN_ERR_TYPE, 
		L_SIGN_TRUE, L_CRITICAL, L_STATISTIC_AI_ERR, L_MESSAGE_TRUE, L_ERR_NOT_TIP, L_ONLY_BOT, L_CANCEL_CLOSE, L_PROMPT_CRITICAL, L_SIGN_TRUE_L, 
		L_ONEKEY_SET_TRUE, L_FRONT_SIZE, L_ONEKEY_SET_NG, L_CODEUNLIKE_TIP, L_DECODE_INFO_LOSE_REMIND, L_CANCEL_COLOR, L_JUMP_TO_ERR,
		L_IMPORTAN_ERR_SET_TRUE, L_SAME_ERR_SIGN_TRUE, L_SET_VIEW_TEXT, L_MOVE_MESSAGE, L_SECOND_CONFIRM,
		//���޲���->��Ԫ
		L_NOT_CANCLE, L_NOT_NEXT, L_SIGN_CANCLE, L_CRITICAL_CANCEL, L_DECIDE_ERR_UNIT, L_HID_TRUERR, L_AUTO_HIDE, L_NOT_HIDE_TRUEU, 
		L_KEEP_MARK_UNIT, L_AUTOUNIT, L_ERR_UNITS_SET, L_STAT_BY_UNIT, L_UUNIT_POINT, L_UNIT_NUM, L_NO_ERR_UNIT_TIP, L_END_IN_OUTPUT, 
		L_SAVE_TO_SCRAP, L_LOAD_ERR_UNIT, L_SCRAP_PATH, L_PRE_ERR_DECODE_PATH,
		//���޲���->AI
		L_HAVE_AI, L_PLNOHAVE_AI, L_SHOWAI_ONLY, L_MISS_VIEW, L_MISS_PATH, L_CHECK_NUM, L_AI_COLOR_SET, L_SHOW_AI_FAKE,
		//���޲���->ȱ������
		L_TOLERANCE_X, L_TOLERANCE_Y, L_SAME_ERR_POS, L_POS_PCB_NUM, L_POS_TOL, L_VERR_LEVEL, L_TERRPIC_SETTING, L_VERR_SHAPE, L_REMIND_PATH, 
		L_REMIND_PATH2, L_AUTO_SAVE_PIC, L_DEFECT_PATH, L_FAKEDIB_PATH, L_FAKEDIB_PATHF, L_AUTO_STATISTIC, L_EAUTO_STATISTIC, L_PL_SAVE_PATH, L_ZOOM_ERR,
		//���޲���->����
		L_PASSWORD_SET, L_VLAN, L_EAP, L_VEAP_SETTING, L_VCAR_SYSTEM, L_LOCK, L_VCHANGE_PASSW, L_VERIFY_SYSTEM, L_WORK_CAR_SYSTEM, L_ERR_EXPORT,
		L_SAVE_TO_DB, L_LOAD_FROM_DB,
		//�����û�����ҳ�棨��ҳ�㣩
		L_PATH, L_OTHER_CAR, L_LIGHT, L_ERR_WND, L_ONLY_VIEW_AI, L_TIME_SPAN, L_HOLE_PARM, L_LOCK_PAGE

	};
	//step2:���Ӳ���L_COUNT��1
	enum { L_COUNT = 534 };		
	//step3:��Ӧҳ��LEN��Ҫ��1
	enum {
		//�ĵ�����
		ERRSIZE = 0, ERRSIZELEN = 36, BLOCKED = ERRSIZE + ERRSIZELEN, BLOCKEDLEN = 18, ADJUST = BLOCKED + BLOCKEDLEN, 
		ADJUSTLEN = 16, PAD = ADJUST + ADJUSTLEN, PADLEN = 26 , RINGHOLE = PAD + PADLEN, RINGHOLELEN = 24,
		LINE = RINGHOLE + RINGHOLELEN, LINELEN = 30, MEASURE = LINE + LINELEN, MEASURELEN = 9, OTHER = MEASURE + MEASURELEN, OTHERLEN = 47,
		FPC = OTHER + OTHERLEN,
		AISPSET = 1,
		FPCLEN = 7 , HOLE = FPC + FPCLEN + AISPSET, HOLELEN = 23,
		//��ɫ���û���F8������
		COLORSET = HOLE + HOLELEN, COLORSETLEN = 9, USERNOR = COLORSET + COLORSETLEN, USERNORLEN = 9,
		USEROTH = USERNOR + USERNORLEN, USEROTHLEN = 46, ADVANCE = USEROTH + USEROTHLEN, ADVANCELEN = 41, REGION = ADVANCE + ADVANCELEN,
		REGIONLEN = 10,
		//����
		JOBSET = ADVANCE + ADVANCELEN + REGIONLEN, JOBSETLEN = 5, DEVICE = JOBSET + JOBSETLEN, DEVICELEN = 19, ERRWND = DEVICE + DEVICELEN, ERRWNDLEN = 34,
		PREFERANCE = ERRWND + ERRWNDLEN, PREFERANCELEN = 66, ERRUNIT = PREFERANCE + PREFERANCELEN, ERRUNITLEN = 20, 
		PAGEAI = ERRUNIT + ERRUNITLEN, PAGEAILEN = 8, ERRSET = PAGEAI + PAGEAILEN, ERRSETLEN = 18, ERROTH = ERRSET + ERRSETLEN, ERROTHLEN = 12,

	};
public:
	BYTE	m_aLevels[L_COUNT];

public:
	void SetLevel(int iPara, int iLevel);
	int GetLevel(int iPara);

	BOOL Serialize(CArchive& ar);
	void Copy(CParaLevel& level);

public:
	//step4:����Ӷ�Ӧ�ַ�����ע��Ҳ��Ӧ����
	BYTE	m_aSelectIndexs[L_COUNT];//��Ҫ����ѡ����������	   //���Ӳ�����Ӧ����λ�õ��ַ�����Ҫ����
	CString DocString[L_COUNT] = {//ȱ�ݴ�С--34
									"ʴ�̲���","��͹ͭ","��͹ͭ���","����͹ͭ���","С���̼�����","С���������","С���̾��ȶ�","�׻�","����","����","���̾��ȶ�",
									"����ͭ","����ͭ���ȶ�","����Ĥƫλ","���Ϳ���","����","���ͻ��ľ��ȶ�","���Ϳױ�","¶ͭ","�ַ�","���ַ�����׼",
									"��·��/������","��Ƭ","FR4","PI","©��","ä��","�����","����������","��������","����ͭ��","���ͻ��Ŀ�","С���̾��ȶ�-����","���̾��ȶ�-����","����ͭ�����ȶ�", "���ͻ��������ȶ�",
									//��Ե���--4
									"ͨ�����","�������","PAD���","�ָ�Ĭ��",
									//��Ե���->����--14
									"����->���Ϳױ߷�Χ","����->����ͭ�߽緶Χ","����->���ͻ��ı߽緶Χ","����->���ı߽緶Χ","����->©��߽����","����->����ױ߽����","����->��߲���","����->©���⾫��",
									"����->ͭƤ��Ե��⾫��","����->����ͭ��Ե��Χ","����->���Ͱ��Ե��Χ","����->�Ƕ�ͭ��", "����-���¶ͭ","����-�������ָб�����",
									//΢��λ--9
									"��λ�����ߴ�","��ʹ�ÿ������͵�PAD��΢��λ","ʹ�ÿ�/��ͭ���λ","ʹ������ͭ���λ","���Ե��λ","���ֵ�����λ","ֻʹ�ð��ֶ�λ","ʵ�ʰ��ֺͷ��ֲ���","�������λ��������",
									//΢��λ->����--7
									"����->ͭƤ��Ե��λ���","����->���ͻ���-���ı�Ե��λ","����->�����λ�Ż�","����->����ָ�������˶�λ","����->�ư�","����->������","����->���̿׻�12��λ��ɫ����",
									//����--8
									"����ä�׻������","���ȶȼ���Ż�","��⺸�����","���ȶȲ�ֵ�����趨ֵ","���BGA���������","��⺸�̱�����","�������Ϻ���","����ܱ�������ͭ���̱���С���",
									//����->����--18
									"����->С����ֵ�ĺ��̲��춪ʧ","����->С����ֵ�ĺ��̲��������","����->ʴ�̶Ͽ��ĺ��̲��춪ʧ","����->��������������ڵ�Բ���̰�͹ͭ","����->��ͬ���������캸������߼�","����->�����̰�͹ͭ",
									"����->������͹��","����->��⺸��С����","����->����ָĥ��","����->��⺸�̸�������","����->��⺸�̵���","����->��⺸������","����->��⺸�̸�������","����->��⺸�̿���������",
									"����->��⺸�ֲ̾�����","����->���С���趨���صĺ���ֱ�߲���ⰼ͹ͭ","����->���̰�����ֵ", "����->�ϴ��̱�����ȱ��",
									//�׺Ϳ׻�--9
									"������ֵ�Ŀ׵�����ͨ����","���Ϳ׻��Աߵ������Ϳױ�","����(��)�����","���⾶��Χ","��Сring","ä�״�С","ä�ױ�Ե","BGAä��PAD����ָ����ɫ","��������ä��",
									//�׺Ϳ׻�->����--15
									"����->���Ƕ�ͨ��","����->����������","����->�����Ϳ�©��(���Ϳ�����ɫ)","����->�����Ϳ�©��(���Ϳױ���ɫ)","����->�����Ϳ�����","����->���񵥶����",
									"����->���׻�����ͭ˿","����->С����ֵ�׻����ÿ׶�λ","����->С����ֵ�׻�������ʧ","����->���ױ�","����->�׻��ھ�����ױ�","����->��С����ɫ���������¿ױڷ�Χ", "����->�׸������ȶ����", "����->С����ֵ�׻�����",
									"����->�����׽Ƕ�",
									//��·--30
									"�����·����","�����·������","������·�䰵��","������ָ������","������·�ս�","������·��ս�","�����¶��·","�߿���Χ","��С�߳�","��·�߽��ֵ",
									"��¶��·�߽��ֵ","��·�������ֵ","��·�������ֵ","����ָ�������ֵ","��¶��·�����ֵ","��⺸�̸�����·","�����·�Ͽ�","���������","��·����������α��ٵ�","����߿�",
									"�������·","�����¶��·����","�����·΢ͻ��","�����·΢����","�����������","�����Ϳ�����","����������","���������Ҳ���������߼���","�в���������ͭ�������Ų�������",
									"��·ʴ��",
									//����--9
									"���Tiebar","����ͷ��ƫ","��⺸�̳���","�������","��⺸�̼��","��⿪������","�׼�����","��������ƫλ","����Mark��ͬ�Ķ�",
									//����--28
									"���ƫλ","����ƫλ","����ƫλ","ͭ��������Щ","����ָ��Щ","���˻ҳ�","������ģ��","���ʴ�̳�����������Ŀն�","��⿿�����ֵ�����","��⺸��֮������������",
									"���ļ��¶ͭ","���׷�Χ","V-Cut","��Ԫ����ȱ������׼","ָ��ɨ������","����ʱ��","�������е�����ͭ��","Ĥ��","��ī��ɫ","ȱ�ݵȼ�","�Զ����ɲ���",
									"ѡ������ļ�","��������ļ�","�����ֶ�ʧ", "��⺸�̿�����������������", "������","���������С�İ��ֲ�����","ֻ���������","������",
									//����->�˶��趨--7
									"�˶��趨->̨��Ű�����ƫ��","�˶��趨->���","�˶��趨->�Ű�λ��΢��","�˶��趨->�����������","�˶��趨->Ť���ٷֱ�","�˶��趨->�����ٷֱ�","�˶��趨->ѡ�������Ϊ��������",
									//����->��ά���趨--11
									"��ά���趨->����ʶ��ɹ�ͼ","��ά���趨->����ʶ��ʧ��ͼ","��ά���趨->��Ϊ����","��ά���趨->DM�루��ΪQR�룩","��ά���趨->��ά������","��ά���趨->��ʱʱ�䵥λ","��ά���趨->Grid����",
									"��ά���趨->���ά��ȱ��","��ά���趨->������","��ά���趨->��ά���Ե��ֵ","��ά���趨->ռ��",
									//һ��->ai�ر�ȱ����������--1
									"AI�ر�ȱ����������",
									//�������--7
									"��ǿ��λ����","����Ƭƫλ","����ƫ","��⸲��Ĥƫλ","��ǿ����ƫ��","���Ĥ�߽����","ʹ�ñ߽����",
									//��ײ���--13
									"���׷�Χ","���1","���2","���3","���4","���5","��������ȱ�ݴ�С","���׶��ꡢ����","���ۿ״�С(������״)","���ۿ�ë��",
									"��������","����۱�Ե",
									//��ײ���->�߼�--11
									"�߼�->��ͭ�����1","�߼�->��ͭ�����2","�߼�->��ͭ�����3","�߼�->��ͭ�����4","�߼�->��ͭ�����5","�߼�->��ͭ�����1","�߼�->��ͭ�����2","�߼�->��ͭ�����3","�߼�->��ͭ�����4",
									"�߼�->��ͭ�����5","�߼�->�ۿ�",
		
									//��ɫ�趨--9
									"��ɫ","��ֵ","������","��������","��Դ����","��ƹ�����","���Ƶ���һ��","�ӿ�ѡ��","���浽��",
									//�û�����->һ��--9
									"����·��","ȱ�ݱ���·��","��־·��","���ݷ�������"," ���޴�ͼ","��������·��","��ά�뵥������","��ά��·��","������ɨ��ǹ",
									//�û�����->����--39
									"ȱ�ݴ�������","ȱ��ͼƬ��С","���ȱ����","ȱ�ݴ������Է��α�ʾλ��","ȫ�ִ���ȱ�ݱ�ʾ","��ʾ��������","�̶�����ʾ","����ɨ��ָ�����","��ʾ����ȱ��","NG������","������һ��������������","˫Ͷ�ϰ���","����������Ϣ",
									"ȱ����������","ÿ����Ƭ��","����������ͣ","��̨�������","��̨��������","�Զ���ӡ","���Խ��������ʾ","©���PASS̨��","����©�����","NG���PASS̨��","PASS���FAIL̨��","��ά��ʧ�ܵ�NG","�����NG̨��","���˫�����ƶ���ͷ",
									"��ά������Ǽ�","���벻����","��ά�����뱨������","ȱ�ݺϲ�","���趨�Ǽ�͹���ʱ��ʾ�û�","�ֶ�����ά��ʶ��ʧ��ʱ���û��ֶ������ά��","�����ͣ��","NG�岻�󶨶�ά����Ϣ","ͬʱ������Ͷ�ά��","�û�������������","�ϲ�����λ�ú�",
									"��ѡ���ֲ�ͬʱ����","��Ԫ����",
									//�û�����->����->Debug��ͼ����--6				
									"Debug��ͼ->�ܰ������ͼ","Debug��ͼ->�ܰ��ԭͼ","Debug��ͼ->����NG��ͼ","Debug��ͼ->���в��Գ�ʱ��ͼ","Debug��ͼ->���б�����ͼ","Debug��ͼ->�����ͼ��ֵ",
									//�߼��趨--41
									"����","���ؾ���","����̨������X","����̨������X","����̨�����","����̨�����","CCD����","ʹ�ñ���IP","�Ϸ�Ԥ������",
									"����","ֻɨ���������","ʹ�ò�����Χ","ȱ�ݴ���㹦��","������Χ����ȡ��","�Զ����","���ʹ�����ʹ���ڴ�ģ��Ӳ��",
									"CCD�ɷ���","ƴ�浫һ����ά��","bmp��ʽȱ��","Pass�����⴦��","��⼤��ä��","֧��Eapϵͳ","����","ֻ��������ɨ��",
									"���ӿ������Ű�","��AIϵͳ","��ʾAI���","AI IP","�������õ����ڵİ���","CCDƫ�ƾ���","������IP��ַ","��׼�����Ӳ������",
									"����ģʽ","���","����ϵͳ","��������","�˶�����","����ͬʱ�Բ�","���ز���Gerber","�������","�����ͼ",
									//����--10
									"����","����","�Ǽ���","��-ͭ��","��-����","������","������","PAD","�޸�","�ؼ�����",

									//����count 179
									//���޲���->��������--5
									"����·��","ȱ�ݱ���·��","��־·��","��ά�뵥������","��ά��·��",
									//���޲���->�豸�趨--19
									"��ǰ��̨��","�����豸����","��̨IP��·������","�Ҽ��ƶ����","CCD�ɷ���","�������","���˫�����ƶ���ͷ","������","��ά���������","ɨ������",
									"�м���װ��","��ɨ��ǹ","ɨ��ǹ���Ӷ�ά����Ϣ","������������ƴ��","ֻɨ��������","��ͶӰ��","ͶӰ��","���������","���ö��������",
									//���޲���->ȱ�ݴ�������--34
									"�������","�������","��ʾ��״","��ʾ�߿�","ȱ��ͼ��Ĭ�Ͼ���","������ɫ","ȱ�����ͱ�������","ȫ������","ȱ������ͬʱ��ʾ",
									"�����趨","���׿̶ȳ�","��Ԫ�̶ȳ�","��߿̶�","���趨ֵд�̶�","ȱ��ͼ����Gerber������ʾ","������ʾ","ȱ��ͼ����","ȱ��ͼ���м�",
									"ȱ��ͼ����","ȱ��ͼ������","ʹ��Gerber�Ա�","����Ԥ��","ʵʱͼ��ʾ��Ԫλ��","Gerber��ʾ","ȱ����ʾ�����","ȱ�ݼ�����ʾ","�����ʾ�ڵ�һ��",
									"Gerber��ʾ�����ͼƬ���","ȱ��ͼ��ʾ����ռȫ�ִ��ں������","ʵʱ������ʾ��׼ͼ","ͼ����ʾ","���ϴ��ڿ���","Gerber��ɫ����","��ʾ����",
									//���޲���->ƫ��--66
									"����","����","�嵥Ԫ˳��","����˳��","Ĭ��չ���û������б�","��������б�","����ͬʱ��ʾ","��ת90","ֻ��ʾ����","��Gerber��Ԫ����",
									"Z������","������","��Ԫ����������","NgPcs","ȷ�϶Ի�������","���ÿ�ݼ�","ȱ��������ʾ","��ά��У��","���ڷ���","��һ��ʱ����",
									"ȱ�ݷ�ҳ��С���ʱ��","ȱ�ݷ�Ƭ��С���ʱ��","ȱ�ݱ�ǩ��С","��Ԥ��","������ϵ�","���趨Ԥ��","Ԥ������","Ԥ������","Ԥ������",
									"����ȷ��","ȷ��ǰ","ȷ�Ϻ�","��ҳ����Ƭ","������ҳ��","����End��","End �������","������","ȡ����һ�浯��","���˼�¼��u����","��������",
									"NG��ʾȱ��","�Ѽ��޹�����","���뵯��","ä����ǰ����","���ּ����ȱ��","���ȱ�ݴ����","����ȱ��©���ʱ������ʾ","��Ai���ȱ�����ͼ���ͳ��",
									"�������","ȱ�ݲ�����ȷ��","ֻ�з���","������رյ���","����ȱ������","��������","һ�������","ȱ�������С","�̶�����Ϊ���","���ϵ�Ԫ��ɫ�û�","���ּ�������ǰȱ��","����ȱ����Ϊ���","һ��ȷ��NG","������ά�벻һ������","��ά����Ϣȱʧ����","ȱ����ʾ����","������������","����ȷ��",
									//���޲���->��Ԫ--20
									"���ϲ���ȡ��","���ϲ�����һ��","��Ǵ򱨷�","����ȱ�ݴ򱨷�","Ĭ�ϲ�ѡ�����ϵ�Ԫ","������㵥Ԫ","�Զ����ε�Ԫ","�������в�������㵥Ԫ",
									"������㵥Ԫ","�Զ�������㵥Ԫ","���ϵ�Ԫ����","�����¼NG��Ԫ��","��Ԫ����","�����Ԫ���","���򱨷�ʱ��������","���ϵ�Ԫ�ļ�������END�ֶ�",
									"д������Ա��Ϣ�����ϵ�Ԫ�ļ�","�����ز�屨�ϵ�Ԫ","���ϵ�Ԫ·��","ǰ�˱��϶�ά��·��",
									//���޲���->AI--8
									"��AIϵͳ","������AI�ļ�����","ֻ��AI","AI ©ʧ��������","AI©ʧͼƬ·��","ÿ�����ǰ","AI��ɫ�趨","ֻ��AI�ٵ�",
									//���޲���->ȱ������--17
									"�ݲ�X","�ݲ�Y","�̶�����ʾ","�Ƚ�ǰ��Ƭ","ͬһλ�����","ȱ�ݵȼ��趨","���ϵ�洢ͼ������","ȱ�ݱ�ʾ��״","�ر�ָ��·��","�ر�ָ��·��2",
									"���������Զ��洢ȱ��ͼ","ȱ��ͼ·��","�ٵ�ͼƬ�洢","F11��F12�ٵ�ͼƬ�洢","ȱ���Զ�ͳ��","ͳ�ƽ���洢·��","�����洢·��","ָ��ȱ�ݷŴ���ʾ",
									//���޲���->����--10
									"�����趨","����","Eap","Eap����","����ϵͳ","����/����","�޸�����","��֤ϵͳģʽ","�Զ���������","ȱ�ݱ�������",
									//���浽��
								"���浽��", "�ӿ�ѡ��",
	};
};



/* ===================== 原文件：paraleveldlg.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>
#include <afxcmn.h>
// [已合并依赖] #include "TabSheet.h"
// [已合并依赖] #include "ErrSizeLevel.h"
// [已合并依赖] #include "BlockDeLevel.h"
// [已合并依赖] #include "AdjustLevel.h"
// [已合并依赖] #include "PadLevel.h"
// [已合并依赖] #include "RingHoleLevel.h"
// [已合并依赖] #include "LineLevel.h"
// [已合并依赖] #include "MeasureLevel.h"
// [已合并依赖] #include "OtherLevel.h"
// [已合并依赖] #include "ColorLevel.h"
// [已合并依赖] #include "UserNorLevel.h"
// [已合并依赖] #include "UserOthLevel.h"
// [已合并依赖] #include "AdvanceLevel.h"
// [已合并依赖] #include "JobCarLevel.h"
// [已合并依赖] #include "DeviceLevel.h"
// [已合并依赖] #include "ErrWndLevel.h"
// [已合并依赖] #include "PreferanceLevel.h"
// [已合并依赖] #include "ErrUnitLevel.h"
// [已合并依赖] #include "PageAiLevel.h"
// [已合并依赖] #include "ErrSetLevel.h"
// [已合并依赖] #include "ErrOthLevel.h"
// [已合并依赖] #include "FPCLevelDlg.h"
// [已合并依赖] #include "HoleLevelDlg.h"
// [已合并依赖] #include "RegionDlgLevel.h"

// CParaLevelDlg �Ի���

class CParaLevel;

class CParaLevelDlg : public CDialog
{
	DECLARE_DYNAMIC(CParaLevelDlg)

public:
	CParaLevelDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CParaLevelDlg();

// �Ի�������
	enum { IDD = IDD_PARA_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:

	CParaLevel*	m_pParaLevel;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnTcnSelchangeTabDocsetting(NMHDR *pNMHDR, LRESULT *pResult);

//	CCheckListBox m_DocList;
//	int m_iSetLevel;
//	BOOL m_bSelAll;

//	afx_msg void OnLbnSelchangeList1();
//	afx_msg void OnCbnSelchangeComboSetlevel();
//	afx_msg void OnBnClickedCheckSelall();

//	CCheckListBox m_ColorList;
//	afx_msg void OnSelchangeListColorpara();
//	afx_msg void OnCbnSelchangeComboSetlevel2();
//	afx_msg void OnBnClickedCheckSelall2();
//	int m_iSetLevel2;
//	BOOL m_bSelAll2;

	CTabSheet m_TabDocSetting;//��ǩҳ����
	CTabSheet m_TabColorSetting;
	CTabSheet m_TabViewSetting;
	//�ӶԻ���
	CErrSizeLevel    m_ErrSizeLevel;
	CBlockDeLevel    m_BlockDeLevel;
	CAdjustLevel     m_AdjustLevel;
	CPadLevel	     m_PadLevel;
	CRingHoleLevel   m_RingHoleLevel;
	CLineLevel       m_LineLevel;
	CMeasureLevel    m_MeasureLevel;
	COtherLevel      m_OtherLevel;
	CColorLevel      m_ColorLevel;
	CUserNorLevel    m_UserNorLevel;
	CUserOthLevel    m_UserOthLevel;
	CAdvanceLevel    m_AdvanceLevel;
	CJobCarLevel     m_JobCarLevel;
	CDeviceLevel     m_DeviceLevel;
	CErrWndLevel     m_ErrWndLevel;
	CPreferanceLevel m_PreferanceLevel;
	CErrUnitLevel    m_ErrUnitLevel;
	CPageAiLevel     m_PageAiLevel;
	CErrSetLevel     m_ErrSetLevel;
	CErrOthLevel     m_ErrOthLevel;
	CFPCLevelDlg     m_FPCLevelDlg;
	CHoleLevelDlg    m_HoleLevelDlg;
	CRegionDlgLevel  m_RegionDlgLevel;
	int m_iSaveToDB;
	int m_iLoadFromDb;
};



/* ===================== 原文件：levelsetting.h ===================== */
// LevelSetting.h: interface for the CLevelSetting class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LEVELSETTING_H__51A0469F_7378_493F_8A2E_D441450D8E77__INCLUDED_)
#define AFX_LEVELSETTING_H__51A0469F_7378_493F_8A2E_D441450D8E77__INCLUDED_

#if _MSC_VER > 1000
// [原文件 pragma once：聚合后由本文件统一保护]
#endif // _MSC_VER > 1000

class AFX_EXT_CLASS CLevelSetting  
{
public:
	CLevelSetting();
	virtual ~CLevelSetting();

	int		m_nCopperDilation;		// ͭʴ�̲���
	int		m_nSolderCopperDilation;		// �躸ͭʴ�̲���
	BOOL	m_bCheckEdge;			// ���SMDֱ��
	BOOL    m_bCheckEdgeInter;      // ��ֵ���SMDֱ��
	int		m_nCheckEdgeLen;
	int		m_nCheckEdgeDiaLen;
	int		m_nEdgeTol;
	int		m_nEdgeTol_Circle;
	int		m_nSmdRingEdgeTol;
	int		m_nSmdPercent[2];
	int		m_nErrSmdMinWidth;

	int		m_nErrSize_hole;
	int		m_nErrSize_mmat;
	int		m_nErrSize_mcopper;
	int		m_nErrSize_ring;
	int		m_nErrSize_silk;
	int		m_nErrSize_smd;
	int		m_nErrSize_mring;
	int		m_nErrSize_silkBig;
	int		m_nErrSize_silkBigJY;
	int		m_nSilkThreshold;		// ���ִ�С��׼�����ڸñ�׼��Ϊ�����

	int     m_nBigVoidTh;
	int     m_nErrSize_BigVoid;

	int     m_nErrSize_Sheet;

	int		m_nErrSize_bigcopper;
	int		m_nErrSize_lutong;
	int		m_nErrSize_Line;
	int		m_nErrSize_LineSpace;
	int		m_nErrSize_Void;
	int     m_nErrSize_DarkMaskEdge;
	int     m_nErrSize_Pi;
	int     m_nErrSize_LightMaskEdge;
	int     m_nErrSize_Blind;
	int     m_nErrSize_BackDrl;
	int     m_nErrSize_MCOffset;

	int		m_nErrSize_JYsmd;
	int		m_nErrSize_JYbcopper;
	int		m_nErrSize_JYmcopper;
	int		m_nErrSize_JYmmat;
	int		m_nErrSize_JYmat;

	int		m_nErrSize_JYmcopperLight;
	int		m_nErrSize_JYmmatLight;

	int		m_nErrSize_JYsmdDark;
	int		m_nErrSize_JYbcopperDark;
	int		m_nMaskBroken;
	int		m_nErrSize_MCopperOrMMat;
	int		m_nErrSize_MaskPcbEdge;
	int		m_nErrSize_MaskOrMat;
	int		m_nErrSize_mat;
	int		m_nErrSize_PadOpeningThickness;
	int		m_nGbPadOpeningThickMin;

	int 	m_nErrSize_MaskBridge;

	int     m_nErrSize_MaskHole;
	int		m_nMaskCopperBlock;
	int		m_nMaskMatBlock;

	int		m_nErrSizeMaskLineExposedCop;

	//����ͬ��
	int     m_nDarkDiff;
	int     m_nLightDiff;
	int		m_nLargeCopperContrastLightCv;
	int		m_nLargeCopperContrastDarkCv;
	int		m_nCopperAreaCv;
	int		m_nLargeCopperAreaCv;
	int		m_nCopperContrastLightCv;
	int		m_nCopperContrastDarkCv;
	int		m_nCopperIgnore;
	int     m_nSteelErrSize;
	int		m_nCopperNoVoidUp;
	int		m_nCopperNoVoidDown;
	int		m_nCopperNoVoidLR;
	int		m_nCopperNearVoidUp;
	int		m_nCopperNearVoidDown;
	int		m_nCopperNearVoidLR;
	int     m_nErrSize_TC_Mat;
	int		m_nCopperConOuter;
	float	m_fSmdRingEdgeSize;
	int		m_nCopperOuterLightCon;
	int		m_nCopperOuterDarkCon;
	int		m_nBigCopperNearBHoleLightCon;
	int		m_nBigCopperNearBHoldeDarkCon;
	int		m_nSmallCopperNearBHoleLightCon;
	int		m_nSmallCopperNearBHoleDarkCon;
public:
	BOOL Serialize(CArchive& ar);
	//�����³�Ա��Ҫ�޸ĸú��� ahw
	void ModifyData(const CLevelSetting* item1, const CLevelSetting* item2, double dEps);
	//! @brief �ȶ� pBefore/pAfter ���죬����ֶ�д pAfter ֵ��δ����ֶ�д INT_MIN �ڱ�
	void CompareData(const CLevelSetting* pBefore, const CLevelSetting* pAfter);
};

#endif // !defined(AFX_LEVELSETTING_H__51A0469F_7378_493F_8A2E_D441450D8E77__INCLUDED_)


/*
==================================================================================
                                五、等级页面对话框
==================================================================================
*/

/* ===================== 原文件：AdjustLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;
// CAdjustLevel �Ի���

class CAdjustLevel : public CDialog
{
	DECLARE_DYNAMIC(CAdjustLevel)

public:
	CAdjustLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CAdjustLevel();

// �Ի�������
	enum { IDD = IDD_ADJUST_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_AdjustList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：AdvanceLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CAdvanceLevel �Ի���

class CAdvanceLevel : public CDialog
{
	DECLARE_DYNAMIC(CAdvanceLevel)

public:
	CAdvanceLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CAdvanceLevel();

// �Ի�������
	enum { IDD = IDD_ADVANCE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_AdvanceList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：BlockDeLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CBlockDeLevel �Ի���

class CBlockDeLevel : public CDialog
{
	DECLARE_DYNAMIC(CBlockDeLevel)

public:
	CBlockDeLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CBlockDeLevel();

// �Ի�������
	enum { IDD = IDD_BLOCKDE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_BlockDeList;
	BOOL m_bSelAll;
	int m_iSetLevel;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnBnClickedCheckSelall();
	afx_msg void OnCbnSelchangeComboSetlevel();
};


/* ===================== 原文件：ColorLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CColorLevel �Ի���

class CColorLevel : public CDialog
{
	DECLARE_DYNAMIC(CColorLevel)

public:
	CColorLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CColorLevel();

// �Ի�������
	enum { IDD = IDD_COLOR_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();

	CCheckListBox m_ColorList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;
};


/* ===================== 原文件：DeviceLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CDeviceLevel �Ի���

class CDeviceLevel : public CDialog
{
	DECLARE_DYNAMIC(CDeviceLevel)

public:
	CDeviceLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDeviceLevel();

// �Ի�������
	enum { IDD = IDD_DEVICE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_DeviceList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：ErrOthLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CErrOthLevel �Ի���

class CErrOthLevel : public CDialog
{
	DECLARE_DYNAMIC(CErrOthLevel)

public:
	CErrOthLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CErrOthLevel();

// �Ի�������
	enum { IDD = IDD_ERROTH_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_ErrOthList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：ErrSetLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CErrSetLevel �Ի���

class CErrSetLevel : public CDialog
{
	DECLARE_DYNAMIC(CErrSetLevel)

public:
	CErrSetLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CErrSetLevel();

// �Ի�������
	enum { IDD = IDD_ERRSET_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_ErrSetList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：ErrSizeLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

// CErrSizeLevel �Ի���
class CParaLevel;

class CErrSizeLevel : public CDialog
{
	DECLARE_DYNAMIC(CErrSizeLevel)

public:
	CErrSizeLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CErrSizeLevel();

// �Ի�������
//#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ERRSIZE_LEVEL };
//#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	//CCheckListBox m_ErrSizeList;
	BOOL m_bSelAll;
	int m_iSetLevel;
	CParaLevel*	m_pParaLevel;
	CCheckListBox m_ErrSizeLevel;

	//afx_msg void OnLbnSelchangeList1();
	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList2();
	afx_msg void OnBnClickedCheckSelall();
	afx_msg void OnCbnSelchangeComboSetlevel();
};


/* ===================== 原文件：ErrUnitLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CErrUnitLevel �Ի���

class CErrUnitLevel : public CDialog
{
	DECLARE_DYNAMIC(CErrUnitLevel)

public:
	CErrUnitLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CErrUnitLevel();

// �Ի�������
	enum { IDD = IDD_ERRUNIT_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_ErrUnitList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：ErrWndLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CErrWndLevel �Ի���

class CErrWndLevel : public CDialog
{
	DECLARE_DYNAMIC(CErrWndLevel)

public:
	CErrWndLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CErrWndLevel();

// �Ի�������
	enum { IDD = IDD_ERRWND_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_ErrWndList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：FPCLevelDlg.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CFPCLevelDlg �Ի���

class CFPCLevelDlg : public CDialog
{
	DECLARE_DYNAMIC(CFPCLevelDlg)

public:
	CFPCLevelDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CFPCLevelDlg();

// �Ի�������
	enum { IDD = IDD_FPCLEVEL_DLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_FPCList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：HoleLevelDlg.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;
// CHoleLevelDlg �Ի���

class CHoleLevelDlg : public CDialog
{
	DECLARE_DYNAMIC(CHoleLevelDlg)

public:
	CHoleLevelDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CHoleLevelDlg();

// �Ի�������
	enum { IDD = IDD_HOLEVEL_DLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_HoleList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：JobCarLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CJobCarLevel �Ի���

class CJobCarLevel : public CDialog
{
	DECLARE_DYNAMIC(CJobCarLevel)

public:
	CJobCarLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CJobCarLevel();

// �Ի�������
	enum { IDD = IDD_JOBCAR_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_JobCarList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：LineLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CLineLevel �Ի���

class CLineLevel : public CDialog
{
	DECLARE_DYNAMIC(CLineLevel)

public:
	CLineLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CLineLevel();

// �Ի�������
	enum { IDD = IDD_LINE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	
	CCheckListBox m_LineList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：MeasureLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;
// CMeasureLevel �Ի���

class CMeasureLevel : public CDialog
{
	DECLARE_DYNAMIC(CMeasureLevel)

public:
	CMeasureLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CMeasureLevel();

// �Ի�������
	enum { IDD = IDD_MEASURE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_MeasureList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：OtherLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;
// COtherLevel �Ի���

class COtherLevel : public CDialog
{
	DECLARE_DYNAMIC(COtherLevel)

public:
	COtherLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~COtherLevel();

// �Ի�������
	enum { IDD = IDD_OTHER_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_OtherList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：PadLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CPadLevel �Ի���

class CPadLevel : public CDialog
{
	DECLARE_DYNAMIC(CPadLevel)

public:
	CPadLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CPadLevel();

// �Ի�������
	enum { IDD = IDD_PAD_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	BOOL m_bSelAll;
	int m_iSetLevel;
	CCheckListBox m_PadList;
	CParaLevel*	m_pParaLevel;

	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：PageAiLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CPageAiLevel �Ի���

class CPageAiLevel : public CDialog
{
	DECLARE_DYNAMIC(CPageAiLevel)

public:
	CPageAiLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CPageAiLevel();

// �Ի�������
	enum { IDD = IDD_PAGEAI_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()

public:
	CCheckListBox m_PageAiList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：PreferanceLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CPreferanceLevel �Ի���

class CPreferanceLevel : public CDialog
{
	DECLARE_DYNAMIC(CPreferanceLevel)

public:
	CPreferanceLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CPreferanceLevel();

// �Ի�������
	enum { IDD = IDD_PREFERANCE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_PreferanceList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：RegionDlgLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CRegionDlgLevel �Ի���

class CRegionDlgLevel : public CDialog
{
	DECLARE_DYNAMIC(CRegionDlgLevel)

public:
	CRegionDlgLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CRegionDlgLevel();

// �Ի�������
	enum { IDD = IDD_REGIONDLG_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_RegionList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/* ===================== 原文件：RingHoleLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;
// CRingHoleLevel �Ի���

class CRingHoleLevel : public CDialog
{
	DECLARE_DYNAMIC(CRingHoleLevel)

public:
	CRingHoleLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CRingHoleLevel();

// �Ի�������
	enum { IDD = IDD_RINGHOLE_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_RingHoleList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel* m_pParaLevel;

	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
	virtual BOOL OnInitDialog();
};


/* ===================== 原文件：UserNorLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CUserNorLevel �Ի���

class CUserNorLevel : public CDialog
{
	DECLARE_DYNAMIC(CUserNorLevel)

public:
	CUserNorLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CUserNorLevel();

// �Ի�������
	enum { IDD = IDD_USERNOR_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_UserNorList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;
	
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
	virtual BOOL OnInitDialog();
};


/* ===================== 原文件：UserOthLevel.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#include <afxwin.h>

class CParaLevel;

// CUserOthLevel �Ի���

class CUserOthLevel : public CDialog
{
	DECLARE_DYNAMIC(CUserOthLevel)

public:
	CUserOthLevel(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CUserOthLevel();

// �Ի�������
	enum { IDD = IDD_USEROTH_LEVEL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_UserOthList;
	int m_iSetLevel;
	BOOL m_bSelAll;
	CParaLevel*	m_pParaLevel;

	virtual BOOL OnInitDialog();
	afx_msg void OnLbnSelchangeList1();
	afx_msg void OnCbnSelchangeComboSetlevel();
	afx_msg void OnBnClickedCheckSelall();
};


/*
==================================================================================
                                六、用户、登录与授权入口
==================================================================================
*/

/* ===================== 原文件：user.h ===================== */
// User.h: interface for the CUser class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_USER_H__A324353F_CE38_4211_BFF4_49FA9AEE47D0__INCLUDED_)
#define AFX_USER_H__A324353F_CE38_4211_BFF4_49FA9AEE47D0__INCLUDED_

#if _MSC_VER > 1000
// [原文件 pragma once：聚合后由本文件统一保护]
#endif // _MSC_VER > 1000

// [已合并依赖] #include "ParaLevel.h"

class AFX_EXT_CLASS CUser  
{
public:
	CUser(CString& sPath);
	virtual ~CUser();

public:
	class ITEM
	{
	public:
		ITEM(){ sName = ""; sPassword = ""; nEditable = 0; nLevel = 0; }
		CString	sName;
		CString sPassword;
		UINT nEditable;
		int nLevel;
	};
	CPtrArray		m_aItems;
	BOOL            m_bIsView;//�Ƿ����

	enum{ E_COLOR, E_DOC };

private:
	CFile*			m_pLockFile;
	CString			m_sPath;
	CString			m_sAdminPassword;
	CParaLevel*		m_pParaLevel;

public:
	BOOL GetUserNames(CStringArray& aNames, BOOL bIncludeAdmin);
	BOOL Lock();
	void Unlock();
	void ResetItems();

	BOOL GetPassword(CString& sUser, CString& sPassword);
	BOOL SetPassword(CString& sUser, CString& sPassword);

	BOOL GetEditable(CString& sUser, UINT& bEditable, int& nLevel);
	BOOL SetEditable(CString& sUser, UINT bEditable, int nLevel);

	void CopyItem(ITEM* pDest, ITEM* pSrc);
	BOOL Load();
	BOOL Save();

	CParaLevel* GetParaLevel(){ return m_pParaLevel; }
	ITEM* FindItem(CString& sUser);

private:
	BOOL Serialize(CArchive& ar);

	BOOL DoGetPassword(CString& sUser, CString& sPassword);
	BOOL DoSetPassword(CString& sUser, CString& sPassword);

	BOOL DoGetEditable(CString& sUser, UINT& nEditable, int& nLevel);
	BOOL DoSetEditable(CString& sUser, UINT nEditable, int nLevel);
};

#endif // !defined(AFX_USER_H__A324353F_CE38_4211_BFF4_49FA9AEE47D0__INCLUDED_)


/* ===================== 原文件：logindlg.h ===================== */
#if !defined(AFX_LOGINDLG_H__4775419F_13B0_4F20_B8A6_A4D677FB32CD__INCLUDED_)
#define AFX_LOGINDLG_H__4775419F_13B0_4F20_B8A6_A4D677FB32CD__INCLUDED_

#if _MSC_VER > 1000
// [原文件 pragma once：聚合后由本文件统一保护]
#endif // _MSC_VER > 1000
// LoginDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLoginDlg dialog

class CUser;
class CAviBaseDlg;

class CLoginDlg : public CDialog
{
// Construction
public:
	CLoginDlg(CWnd* pParent = NULL);   // standard constructor

	CUser*	m_pUser;
	CStringArray	m_aNames;
	CString	m_sName;
// Dialog Data
	//{{AFX_DATA(CLoginDlg)
	enum { IDD = IDD_LOGIN };
	CComboBox	m_combo;
	int		m_iName;
	CString	m_sPassword;
	BOOL	m_bChangePassword;
	BOOL    m_bHaveWorkCar;
	BOOL    m_bForceLogin;
	UINT    m_nValidatedEditable;
	int     m_nValidatedLevel;
	DWORD	nTime;
	CAviBaseDlg *m_pMainWnd;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLoginDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLoginDlg)
	virtual void OnOK();
	virtual void OnCancel();
	void CompleteLogin();
	afx_msg void OnClose();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CString m_sWorkCarName;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
public:
	void WriteCurrentUserFile();//������˻�����д���ļ�
	afx_msg void OnCbnDropdownComboName();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LOGINDLG_H__4775419F_13B0_4F20_B8A6_A4D677FB32CD__INCLUDED_)


/* ===================== 原文件：usereditdlg.h ===================== */
#if !defined(AFX_USEREDITDLG_H__1F4317A8_CC85_4876_98A4_30D7BCF87AC7__INCLUDED_)
#define AFX_USEREDITDLG_H__1F4317A8_CC85_4876_98A4_30D7BCF87AC7__INCLUDED_

#if _MSC_VER > 1000
// [原文件 pragma once：聚合后由本文件统一保护]
#endif // _MSC_VER > 1000
// UserEditDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CUserEditDlg dialog

#include <functional>

class CUser;

class CUserEditDlg : public CDialog
{
// Construction
public:
	CUserEditDlg(CWnd* pParent = NULL);   // standard constructor

	CUser*	m_pUser;
	std::function<void(CWnd*)> m_showParamRange;
	afx_msg void OnUserParamRange();
// Dialog Data
	//{{AFX_DATA(CUserEditDlg)
	enum { IDD = IDD_USER_EDIT };
	CListBox	m_wndList;
	CString	m_sName;
	int		m_iList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUserEditDlg)
	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CUserEditDlg)
	afx_msg void OnButtonAdd();
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonDelete();
	virtual void OnOK();
	afx_msg void OnSelchangeList();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bColor;
	BOOL m_bDoc;
	afx_msg void OnBnClickedCheckColor();
	afx_msg void OnBnClickedCheckDoc();
	afx_msg void OnBnClickedButtonParaLevel();
	int m_iLevel;
	afx_msg void OnCbnSelchangeComboLevel();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_USEREDITDLG_H__1F4317A8_CC85_4876_98A4_30D7BCF87AC7__INCLUDED_)


/*
==================================================================================
                                七、等级总控制器
==================================================================================
*/

/* ===================== 原文件：CLevelManagerControler.h ===================== */
// [原文件 pragma once：聚合后由本文件统一保护]
#ifndef CLevelManagerControler_H
#define CLevelManagerControler_H


#include <afxwin.h>
#include <map>
#include <unordered_map>
// [已合并依赖] #include "RcCore.h"

// 前置声明 RcCore 命名空间和类型
namespace rceditor
{
    struct SSingleResourceInfo;
    struct SDialogInfo;
}

//! 控件启用状态追踪（解决等级管控与业务逻辑 EnableWindow 互相覆盖的问题）
struct SControlEnableState
{
    bool bLevelDisabled = false;     // 等级系统要求禁用该控件
    bool bBusinessDisabled = false;  // 业务逻辑要求禁用该控件（由 WM_ENABLE 钩子推断）
    bool bExplicitDynamic = false;
    UINT dynamicDialogId = 0;
    UINT dynamicControlId = 0;
    bool bLevelManaged = false;      // 该控件已被等级系统处理过
};


/*!
    \brief 对话框匹配模式枚举
    用于指定运行时如何匹配窗口与配置中的对话框定义。
    - 窗口标题匹配：适用于非 Unicode 全球语言支持环境
    - 类名匹配：适用于 Unicode 全球语言支持环境（必须）
    - 自动模式：根据系统配置自动选择最佳匹配方式
*/
enum class EDialogMatchMode
{
    Auto = 0,           // 自动选择：根据系统 Unicode 配置决定
    ByClassName = 1,    // 优先使用窗口类名（csClassPath/csClassName）匹配
    ByCaption = 2,      // 优先使用窗口标题（csCaption）匹配
    Both = 3,           // 同时尝试类名和标题匹配（类名优先）
};

class AFX_EXT_CLASS CLevelManagerControler
{
public:
	explicit CLevelManagerControler();
	virtual ~CLevelManagerControler();


public:

	/*!
		\brief 添加资源文件路径，用于查找资源文件
		\param strRcFilePath 资源文件路径
	*/
    void AddRcFilePath(const CString& strRcFilePath);
	/*!
		\brief 添加资源文件名，用于查找资源文件
		\param strRcFileName 资源文件名
		\param rcFileFolder 资源文件所在文件夹，默认为空 空表示当前程序目录
	*/
    void AddRcFileName(const CString& strRcFileName, const CString& rcFileFolder = CString());

	/*!
		\brief 设置配置文件的路径
		\param csFilePath 配置文件的路径
	*/
    void SetJsonFilePath(const CString& csFilePath);

	/*!
		\brief 清空已添加的资源文件路径
	*/
	void ClearRcPaths();


	/*bool LoadJsonFile(const CString& csFilePath);
	bool LoadJsonFile();
	bool SaveJsonFile(const CString& csFilePath);
	bool SaveJsonFile();*/

private:

	//! 判断是否是常用控件
	/*!
		\brief 判断是否是常用控件
		\param csCtrlName 控件名称
		\return true 是常用控件 false 不是常用控件
	*/
    bool IsCommonCtrl(const CString& csCtrlName) const;
	//! 通过窗口名称找对应控件并禁用
	/*!
		\brief 通过控件所在的窗口名找到对应的控件并禁用
		\param pWnd 需要禁用或者启用的控件指针
		\param csCtrlOnWndName 需要禁用或启用的控件所在的窗口
		\warning 此方法要求资源中任意两个窗口名不完全相同
	*/
    void DisableControl(CWnd* pWnd, const CString& csCtrlOnWndName);

	//! 通过控件指针和控件所在窗口指针查找并禁用
	/*!
		\brief 通过控件指针和控件所在窗口指针查找并禁用
		\param pWnd 需要禁用或者启用的控件指针所在窗口的指针
		\param pTargetCtrlWnd 需要禁用或启用的控件指针
		\warning 此方法要求资源中不能存在控件数量、窗口名称及控件IDC完全相同的两个窗口
	*/

	void DisableControl(CWnd*pParent, CWnd*pTargetCtrlWnd);

    //! 辅助函数：获取对话框资源ID
    /*!
        \brief 尝试从窗口指针获取对应的对话框资源ID
        \param pWnd 窗口指针
        \return 对话框资源ID，失败返回 0
    */
    UINT GetDialogResourceID(CWnd* pWnd) const;

    //! 辅助函数：判断指定按钮是否应该被禁用
    /*!
        \brief 检查工具栏按钮是否应该根据等级被禁用
        \param nID 按钮ID
        \return true 应该禁用，false 不应该禁用
        \note 用于 BCG 工具栏按钮的等级控制
    */
    bool ShouldDisableButton(UINT nID) const;

    //! 辅助函数：判断是否应该跳过类似Label的控件
    /*!
        \brief 判断窗口是否是Static或GroupBox等不需要禁用的控件
        \param hwnd 窗口句柄
        \return true 应该跳过，false 不应该跳过
    */
    bool IsSkipLabelLike(HWND hwnd) const;



	//! 显示或构造的时候需要 等级、是否管理员及是否只是查看



public:		//! 必须ui线程调用
	/*!
		\brief 显示等级管理对话框
		\param m_blIsAdmin 是否管理员权限
		\return 对话框返回值
	*/
	INT_PTR ShowLevelManagerDlg(const bool& m_blIsAdmin = false);

	/*!
		\brief 需要显示的界面语言
		\param usLan 语言
		\note
			0 - 中文
			1- 英文
			2- 日文
			3- 韩文
			4- 泰文
			5 - 越南文
			6 - 繁体中文
	*/
	void SetInterfaceLan(const unsigned short& usLan);

    bool RefreshLanguageResources();

	/*!
		\brief 当前是否为管理员
		\blAdmin 是否为管理员
	*/
	void SetIsAdmin(const bool& blAdmin);

	/* \brief 设置当前用户等级 */
	void SetCurUserLevel(const unsigned short& usUserLevel);

    /*!
        \brief 设置已打开资源的共享指针映射
        \param pOpenedResources 指向已打开资源映射的指针（键为文件路径）
        \note 此指针由外部管理，CLevelManagerControler 不负责释放
              关闭 ShowLevelManagerDlg 后应重新调用此方法以同步资源变更
    */
    void SetOpenedResources(std::map<CString, std::shared_ptr<rceditor::SSingleResourceInfo>>* pOpenedResources);

    // 若未通过外部注入 opened resources，则根据指定的二进制路径列表加载并构建一个内部 map
    bool CLevelManagerControler::EnsureOpenedResourcesLoaded();

    /*! 
        \brief 在运行时注册一个动态对话框和其控件到 opened resources 中
        \note 如果对话框 ID 已存在，则仅更新其 m_blIsInLevelManager 与控件的 uiLevelMask（如果提供）；
              如果对话框不存在则创建新对话框并插入到资源中。
        \param nIDD 对话框资源 ID
        \param csCaption 对话框标题（宽字符串）
        \param controls 列表，每项为 (控件ID, 控件文本)
        \param inLevelManager 对话框是否在 LevelManager 中（默认 false）
        \param ctrlMasks 可选的控件掩码列表，对应 controls 的索引（若长度不足则视为 0）
        \return 成功返回 true，失败返回 false
    */
    bool RegisterDynamicDialog(UINT nIDD, const CStringW& csCaption,
        const std::vector<std::pair<UINT, CStringW>>& controls,
        bool inLevelManager = false,
        const std::vector<UINT>& ctrlMasks = std::vector<UINT>());

    //! 辅助函数：检查控件访问权限
    /*!
        \brief 根据 RcCore 中的等级数据检查控件是否可访问
        \param dialogID 对话框资源ID
        \param ctrlID 控件ID
        \param userLevel 当前用户等级
        \return true 可访问，false 不可访问
    */
    bool CanAccessControl(UINT dialogID, UINT ctrlID, unsigned short userLevel) const;



	/*!
		\brief 启动等级管理器
		\param lan 语言类型
	*/
	bool StartLevelManager();

	/*!
		\brief 启用或暂停等级管理器
		\param blIsEnable true 启用等级管理器 false 暂停等级管理器
	*/
	void EnableLevelManager(const bool& blIsEnable);

	/*!
		\brief 停止等级管理器
	*/
	void StopLevelManager();

    /*!
        \brief 启用或禁用BCG工具栏按钮（参考 oldlevel 的实现）
        \param bEnable true 启用工具栏按钮 false 根据等级禁用工具栏按钮
        \note 需要定义 _AFXEXT 宏并链接 BCG 库
    */
    void EnableBCGToolbarBtn(bool bEnable = false);

    /*!
        \brief 设置对话框匹配模式
        \param mode 匹配模式
        \note 默认为 Auto，会根据系统 Unicode 配置自动选择
    */
    void SetDialogMatchMode(EDialogMatchMode mode);

    /*!
        \brief 获取当前对话框匹配模式
        \return 当前匹配模式
    */
    EDialogMatchMode GetDialogMatchMode() const;

    /*!
        \brief 清除对话框查找缓存
        \note 当资源数据变化后应调用此方法刷新缓存
    */
    void InvalidateDialogCache();

    //! 等级/业务双状态位：由等级系统内部调用，统一设置控件最终启用状态
    void SetLevelEnableState(HWND hwnd, bool bDisableByLevel);
    void ApplyDynamicControlLevel(CWnd* window, UINT dialogId, UINT controlId);
    bool CanAccessDynamicControl(UINT dialogId, UINT controlId) const;
    void SetBusinessEnableState(CWnd* window, bool enabled);


private:
    //! 根据 bLevelDisabled 和 bBusinessDisabled 合成最终启用状态并应用到窗口
    void ApplyMergedState(HWND hwnd);
    bool ApplyBoundDynamicControl(CWnd* window);
    void RefreshBoundDynamicControls();
    //! 窗口销毁时清理状态表
    void CleanupControlState(HWND hwnd);

private:
	struct SLevelContrlData;//! 用于提高编译速度
	SLevelContrlData* m_pData;

    //! 统一的对话框匹配函数
    /*!
        \brief 尝试匹配窗口与资源中的对话框定义
        \param pParentWnd 父窗口指针
        \param csWndCaption 窗口标题
        \param csWndClassName 窗口类名
        \return 匹配到的对话框信息指针，未匹配返回 nullptr
    */
    std::shared_ptr<rceditor::SDialogInfo> TryMatchDialog(
        CWnd* pParentWnd,
        const CString& csWndCaption,
        const CString& csWndClassName) const;

    //! 根据匹配到的对话框信息禁用控件
    /*!
        \brief 统一的控件禁用处理
        \param pDlgInfo 对话框信息
        \param pCtrlWnd 控件窗口
        \param ctrlId 控件ID
    */
    void ApplyControlAccess(
        const std::shared_ptr<rceditor::SDialogInfo>& pDlgInfo,
        CWnd* pCtrlWnd,
        UINT ctrlId);

    //! 获取有效的匹配模式
    /*!
        \brief 根据 Auto 模式和系统配置返回实际使用的匹配模式
        \return 实际使用的匹配模式
    */
    EDialogMatchMode GetEffectiveMatchMode() const;

private:
    //! 注册钩子相关
    HHOOK m_hHook;                     // 钩子句柄
    static DWORD _s_tlsIndex;          // 线程本地存储索引用于保存 this 指针
    static LRESULT CALLBACK LevelManagerHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool InstallHook(DWORD dwThreadId); // 安装钩子到指定线程
    void UninstallHook();               // 卸载钩子

    //! 等级/业务双状态位相关
    std::unordered_map<HWND, SControlEnableState> m_mapControlStates; // 控件状态表（key=HWND）
    LONG m_nEnableGuard;               // 重入保护计数器（>0 时 WM_ENABLE 跳过业务判定）
};

#endif 


/*
==================================================================================
    聚合接入说明

    1. 新项目：直接 include "level.h"；MFC 工程只需继续提供 afxwin/afxcmn、
       resource 等工程运行依赖；JSON 读写按需 include GovernLens/utils/JsonUtils.h。
    2. 原项目：本文件保留所有原等级声明，便于先整体迁移，再逐项精炼。
    3. GovernLens 通用层：LevelManager/LevelHost 负责去 MFC 的生命周期和
       宿主接口；本文件中的旧 MFC 类型用于迁移和兼容。
    4. 每次从本聚合头文件移除项目耦合，都要在 LEVEL_EXTRACTION.md 登记。
==================================================================================
*/