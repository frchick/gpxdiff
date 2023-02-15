// gpxdiff.cpp : 
//

#include <stdlib.h>
#include <wchar.h>
#include <algorithm>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"

namespace rx = rapidxml;

//-----------------------------------------------------------------------------
class WPT
{
public:
	WPT(const char *lat, const char *lon, const char *name, const char *ele, const char *time) :
		m_lat(lat),
		m_lon(lon),
		m_name(name),
		m_ele(ele),
		m_time(time)
	{
	}
	WPT(const WPT &wpt) :
		m_lat(wpt.m_lat),
		m_lon(wpt.m_lon),
		m_name(wpt.m_name),
		m_ele(wpt.m_ele),
		m_time(wpt.m_time)
	{
	}

	void setNoChange()
	{
		m_noChange = true;
	}

	bool getNoChange() const
	{
		return m_noChange;
	}

	bool operator<(const WPT& a) const noexcept
	{
		const int c0 = strcmp(m_lat, a.m_lat);
		if(c0 != 0) return (c0 < 0);
		const int c1 = strcmp(m_lon, a.m_lon);
		return (c1 < 0);
	}
	bool operator==(const WPT& a) const noexcept
	{
		const int c0 = strcmp(m_lat, a.m_lat);
		if(c0 != 0) return false;
		const int c1 = strcmp(m_lon, a.m_lon);
		return (c1 == 0);
	}
	bool isSameName(const WPT& a) const noexcept
	{
		return (strcmp(m_name, a.m_name) == 0);
	}

	void print() const
	{
		printf("[%s, %s] %s, %s, %s\n", m_lat, m_lon, m_ele, m_time, m_name);
	}

	void outputWpt(FILE *file, bool remove)
	{
		fprintf(file,
			" <wpt lat=\"%s\" lon=\"%s\" iswarning=\"0\">\n"
			"  <ele>%s</ele>\n"
			"  <time>%s</time>\n"
			"  <name>%s</name>\n"
			"  <cmt></cmt>\n"
			" </wpt>\n",
			m_lat, m_lon,
			m_ele,
			m_time,
			(remove? "-remove-": m_name));
	}

protected:
	const char *m_lat = nullptr;
	const char *m_lon = nullptr;
	const char *m_name = nullptr;
	const char *m_ele = nullptr;
	const char *m_time = nullptr;

	// 変更が無いことフラグ
	bool m_noChange = false;
};

//-----------------------------------------------------------------------------
bool readGpx(rx::xml_document<> &doc, rx::file<> &file, std::vector<WPT> &wpts)
{
	doc.parse<rx::parse_trim_whitespace>(file.data());

	rx::xml_node<>* gpx = doc.first_node("gpx");
	if(gpx == nullptr) return false;

	rx::xml_node<>* wpt = gpx->first_node("wpt");
	while(wpt)
	{
		rx::xml_attribute<> *lat = wpt->first_attribute("lat");
		rx::xml_attribute<> *lon = wpt->first_attribute("lon");
		rx::xml_node<> *name = wpt->first_node("name");
		rx::xml_node<> *ele = wpt->first_node("ele");
		rx::xml_node<> *time = wpt->first_node("time");
		if(lat && lon){
			wpts.emplace_back(
				lat->value(), lon->value(),
				(name? name->value(): ""),
				(ele? ele->value(): ""),
				(time? time->value(): ""));
		}
		wpt = wpt->next_sibling("wpt");
	}

	return true;
}

//-----------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
	if(argc < 3) return 0;

	try
	{
		rx::xml_document<> docBase;
		rx::file<> fileBase(argv[1]);
		std::vector<WPT> wptsBase;

		rx::xml_document<> docNew;
		rx::file<> fileNew(argv[2]);
		std::vector<WPT> wptsNew;

		if(!readGpx(docBase, fileBase, wptsBase))
		{
			printf(">エラー: gpxの読み込み失敗。\"%s\"\n", argv[1]);
			return -1;
		}
		std::sort(wptsBase.begin(), wptsBase.end());
		printf(">基準ファイル: %s\n", argv[1]);

		if(!readGpx(docNew, fileNew, wptsNew))
		{
			printf(">エラー: gpxの読み込み失敗。\"%s\"\n", argv[2]);
			return -1;
		}
		printf(">更新ファイル: %s\n", argv[2]);

		// 比較して差分出力
		for(auto it = wptsNew.begin(); it != wptsNew.end(); it++)
		{
			auto itt = std::lower_bound(wptsBase.begin(), wptsBase.end(), *it);
			while(itt != wptsBase.end() && (*it == *itt))
			{
				// 座標も名前も一致したら、変更なしをマーク
				if(it->isSameName(*itt)){
					it->setNoChange();
					itt->setNoChange();
					break;
				}
				itt++;
			}
		}

		// タツマ王用の差分ファイルに出力
		if(4 <= argc)
		{
			FILE *file = nullptr;
			if(fopen_s(&file, argv[3], "wt") != 0)
			{
				printf(">エラー: 差分ファイルの作成失敗。\"%s\"\n", argv[3]);
				return -1;
			}

			fprintf(file,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
			fprintf(file,
				"<gpx version=\"1.1\" "
				"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
				"xmlns=\"http://www.topografix.com/GPX/1/1\" "
				"xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

			// 同じものがなかった WPT のうち、Base にあるものは削除された
			for(auto it = wptsBase.begin(); it != wptsBase.end(); it++)
			{
				if(!it->getNoChange()) it->outputWpt(file, true);
			}
			// 同じものがなかった WPT のうち、New にあるものは追加された
			for(auto it = wptsNew.begin(); it != wptsNew.end(); it++)
			{
				if(!it->getNoChange()) it->outputWpt(file, false);
			}

			fprintf(file,
				"</gpx>\n");
			fclose(file);

			printf(">差分ファイル: %s\n", argv[3]);
		}
		// 差分ファイルが指定されていなければ、画面に出力
		else
		{
			// 同じものがなかった WPT のうち、Base にあるものは削除された
			printf(">Remove\n");
			for(auto it = wptsBase.begin(); it != wptsBase.end(); it++)
			{
				if(!it->getNoChange()) it->print();
			}
			// 同じものがなかった WPT のうち、New にあるものは追加された
			printf(">Add\n");
			for(auto it = wptsNew.begin(); it != wptsNew.end(); it++)
			{
				if(!it->getNoChange()) it->print();
			}
		}
	}
	catch(const std::runtime_error& e)
	{
		printf(">エラー: %s\n", e.what());
		return -1;
	}
	catch (const rx::parse_error& e)
	{
		printf(">エラー: %s [%s]\n", e.what(), e.where<char>());
		return -1;
	}

	return 0;
}
