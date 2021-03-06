﻿/* HightLight
*/
#include "loader.hpp"
#include "hexrays.hpp"
#include "pro.h"
#include "HightLight.h"
#include "diskio.hpp"

#include "kernwin.hpp"
#include <windows.h>

#define APPNAME "HightLight"
#define HEADKEY_NAME "HeadColor"
#define BODYKEY_NAME "BodyColor"
#define FileNAME "SpBird.ini"

hexdsp_t *hexdsp;
HightLight *pHLight = NULL;

// 检查位置是否在字符串中
static bool checkPosInString(qstring input, int pos) {
  int len = input.size();
  bool isString = false;
  int skip = 0;

  if (pos >= len) return false;

  for (int i = 0; i < len; i++) {
    if (pos == i) return isString;

    if (skip > 0) {
      skip--;
      continue;
    }

    // 判断字符串模式
    if (isString) {
      // 双引号, 结束字符串模式
      if (input[i] == '"') {
        isString = false;
      }
      // 转义符, 跳过下一个
      else if (input[i] == '\\') {
        skip = 1;
      }
    }
    else {
      // 双引号, 开始字符串模式
      if (input[i] == '"') {
        isString = true;
      }
      // 正斜杠, 判断注释
      else if (input[i] == '/') {
        size_t next = i + 1;

        if ((next < len) && (input[next] == '/')) {
          break;
        }
      }
    }
  }

  return false;
}

// 跳过字符串和注释查找
static int findEx(qstring input, char find) {
  int len = input.size();
  bool isString = false;
  int skip = 0;

  for (int i = 0; i < len; i++) {
    if (skip > 0) {
      skip--;
      continue;
    }

    // 判断字符串模式
    if (isString) {
      // 双引号, 结束字符串模式
      if (input[i] == '"') {
        isString = false;
      }
      // 转义符, 跳过下一个
      else if (input[i] == '\\') {
        skip = 1;
      }
    }
    else {
      // 双引号, 开始字符串模式
      if (input[i] == '"') {
        isString = true;
      }
      // 正斜杠, 判断注释
      else if (input[i] == '/') {
        size_t next = i + 1;

        if ((next < len) && (input[next] == '/')) {
          break;
        }
      }
      // 匹配的字符
      else if (input[i] == find) {
        return i;
      }
    }
  }

  return -1;
}

//移除代码行中的注释部分
static qstring removeComment(qstring input) {
  size_t len = input.size();
  bool isString = false;
  size_t skip = 0, last;

  for (size_t i = 0; i < len; i++) {
    last = i;

    if (skip > 0) {
      skip--;
      continue;
    }

    // 判断字符串模式
    if (isString) {
      // 双引号, 结束字符串模式
      if (input[i] == '"') {
        isString = false;
      }
      // 转义符, 跳过下一个
      else if (input[i] == '\\') {
        skip = 1;
      }
    }
    else {
      // 双引号, 开始字符串模式
      if (input[i] == '"') {
        isString = true;
      }
      // 正斜杠, 判断注释
      else if (input[i] == '/') {
        size_t next = i + 1;

        if ((next < len) && (input[next] == '/')) {
          break;
        }
      }
    }
  }

  return input.substr(0, last);
}

static bool findMatchPos(strvec_t *pStrvec, int lineNum, int xPos, int &matchLine, int &matchXpos)
{
  qstring strBuf;
  int pos = xPos - 1;
  int line = lineNum;
  char bMatched = 0;

  tag_remove(&strBuf, (*pStrvec)[line].line.c_str(), MAX_NUMBUF);
  strBuf = removeComment(strBuf);

  if (xPos < strBuf.size()) {
    bMatched = strBuf[pos];
  }

  if (bMatched == ')' || bMatched == '(')
  {
    bool bSearchDown = true; // 是否向下查找
    char bMatch = 0; // 待匹配查找字符
    int nStep = 1;

    if (bMatched == ')')
    {
      bMatch = '(';
      bSearchDown = false;
      nStep = -1;
    }
    else
    {
      bMatch = ')';
    }

    int matchTime = 1;
    bool bMat = false;

    pos = pos + nStep;

    do
    {
      bool isString = false;
      int skip = 0;

      while (pos >= 0 && pos < strBuf.size())
      {
        if (skip > 0) {
          skip--;
          pos = pos + nStep;
          continue;
        }

        if (isString) {
          // 正序搜索
          if (bSearchDown) {
            // 判断内嵌双引号: \"
            if (strBuf[pos] == '\\') {
              skip = 1;
            }
            // 判断字符串模式结束
            else if (strBuf[pos] == '"') {
              isString = false;
            }
          }
          // 倒序搜索
          else {
            // 判断字符串模式结束
            if (strBuf[pos] == '"') {
              int nextPos = pos + nStep;

              // 判断内嵌双引号: \"
              if (nextPos >= 0 && nextPos < strBuf.size()) {
                if (strBuf[nextPos] != '\\') {
                  isString = false;
                }
              }
            }
          }
        }
        else {
          // 非字符串模式, 判断匹配字符
          if (strBuf[pos] == '"') {
            isString = true;
          }
          else if (strBuf[pos] == bMatch)
          {
            matchTime--;
          }
          else if (strBuf[pos] == bMatched)
          {
            matchTime++;
          }

          if (matchTime == 0)
          {
            // 匹配到
            bMat = true;
            break;
          }
        }

        pos = pos + nStep;
      }

      if (bMat)
      {
        matchLine = line;
        matchXpos = pos;
        break;
      }

      // 要换行匹配
      line = line + nStep;

      tag_remove(&strBuf, (*pStrvec)[line].line.c_str(), MAX_NUMBUF);
      strBuf = removeComment(strBuf);

      if (bSearchDown)
      {
        pos = 0;
      }
      else
      {
        pos = strBuf.size() - 1;
      }

    } while (line >= 0 && line < pStrvec->size());

    return bMat;
  }
  else
  {
    // 暂时只支持 ( and )
    return false;
  }

}



static void convert_zeroes(cfunc_t *cfunc);

void get_ini_path(qstring &path)
{
  path = idadir(PLG_SUBDIR);
  path += "\\";
  path += FileNAME;
}

int GetConf(qstring &path, bgcolor_t &headColor, bgcolor_t &bodyColor)
{
  int headValue = GetPrivateProfileIntA(APPNAME, HEADKEY_NAME, 0, path.c_str());
  if (headValue == 0)
  {
    return 1;
  }
  int bodyValue = GetPrivateProfileIntA(APPNAME, BODYKEY_NAME, 0, path.c_str());
  if (bodyValue == 0)
  {
    return 2;
  }
  headColor = headValue;
  bodyColor = bodyValue;
  return 0;
}

int SetConf(qstring &path, qstring &headColor, qstring &bodyColor)
{
  if (WritePrivateProfileStringA(APPNAME, HEADKEY_NAME, headColor.c_str(), path.c_str()) == FALSE)
  {
    int err = GetLastError();
    return err;
  }
  if (WritePrivateProfileStringA(APPNAME, BODYKEY_NAME, bodyColor.c_str(), path.c_str()) == FALSE)
  {
    int err = GetLastError();
    return err;
  }
  return 0;
}

bool GetConfColor(bgcolor_t &headColor, bgcolor_t &bodyColor)
{
  bool ret = false;
  qstring iniPath;
  get_ini_path(iniPath);
  if (!GetConf(iniPath, headColor, bodyColor))
  {
    ret = true;
  }
  return ret;
}

bool SetConfColor(qstring &headColor, qstring &bodyColor)
{
  bool ret = false;
  qstring iniPath;
  get_ini_path(iniPath);
  if (!SetConf(iniPath, headColor, bodyColor))
  {
    ret = true;
  }
  return ret;
}

ssize_t idaapi myhexrays_cb_t(void *ud, hexrays_event_t event, va_list va)
{
  HightLight *pTempHlight = (HightLight *)ud;
  switch (event)
  {
  case hxe_maturity:
    break;
  case hxe_open_pseudocode:
  case hxe_switch_pseudocode:
  case hxe_refresh_pseudocode:
  case hxe_close_pseudocode:
  {
    pTempHlight->clear();
    pTempHlight->ClearColor();
    break;
  }
  case hxe_curpos:
  {
    // 鼠标点击
    vdui_t *vu = va_arg(va, vdui_t*);
    cfuncptr_t* pFun = &vu->cfunc;
    ctree_maturity_t eLevel = (*pFun)->maturity;
    if ((*pFun)->maturity != CMAT_FINAL)
    {
      msg("HightLight: ctree maturity is not CMAT_FINAL!\n");
      msg("ctree_maturity_t level is %d\n", eLevel);
      return 0;
    }
    if (!vu->visible())
    {
      msg("HightLight: pseudocode window is invisible!\n");
      return 0;
    }
    if (!vu->refresh_cpos(USE_KEYBOARD))
    {
      return 0;
    }
    ctext_position_t cPos = vu->cpos;
    // 添加 () [] 等匹配跳转
    int yPos = cPos.lnnum;
    int xPos = cPos.x;
    if (yPos == 0)
    {
      // 第一行函数声明 只有开头和结尾有"\x01\x17"和"\x02\x17"，(和）无Punctuation
      break;
    }
    strvec_t* str_t = (strvec_t*)&(*pFun)->get_pseudocode();
    qstring strBuf;
    int matchLine = 0;
    int matchxpos = 0;
    char selectBuf = 0;

    //TEST 
    //		char *pAdvance = (char *)tag_advance((*str_t)[yPos].line.c_str(), 0);
    //		msg("check tag_advance : %s", pAdvance);
    if (pTempHlight->ResetNoRefresh(str_t))
    {
      refresh_idaview_anyway();
    }

    tag_remove(&strBuf, (*str_t)[yPos].line.c_str(), MAX_NUMBUF);

    strBuf = removeComment(strBuf);

    if (xPos > 1 && xPos < strBuf.size())
    {
      char current = strBuf[xPos - 1];

      // 只选择非字符串中的()
      if ((current == '(' || current == ')') && (checkPosInString(strBuf, xPos) == false)) {
        selectBuf = current;
      }
    }

    if (selectBuf == '(' || selectBuf == ')')
    {
      if (findMatchPos(str_t, yPos, xPos, matchLine, matchxpos))
      {
        if (matchLine == 0)
        {
          return 0;
        }
        char *pAdvBegin = (char *)(*str_t)[yPos].line.c_str();
        int iAdvLen = (*str_t)[yPos].line.size();
        char *pAdvance = (char *)tag_advance(pAdvBegin, xPos - 1);
        char *pMatchBegin = (char *)(*str_t)[matchLine].line.c_str();
        int iMatchLen = (*str_t)[matchLine].line.size();
        char *pMatch = (char *)tag_advance(pMatchBegin, matchxpos);

        while (*pAdvance != COLOR_ON || *(pAdvance + 1) != COLOR_SYMBOL)
        {
          // 解决未知情况下越界问题
          if (pAdvance < &pAdvBegin[iAdvLen - 3])
          {
            pAdvance++;
          }
          else
          {
            // 要越界，直接返回
            return 0;
          }

        }
        while (*pMatch != COLOR_ON || *(pMatch + 1) != COLOR_SYMBOL)
        {
          // 解决未知情况下越界问题
          if (pMatch < &pMatchBegin[iMatchLen - 3])
          {
            pMatch++;
          }
          else
          {
            // 要越界，直接返回
            return 0;
          }
        }
        pTempHlight->InsertColor(yPos, (*str_t)[yPos].line);
        if (yPos != matchLine)
        {
          pTempHlight->InsertColor(matchLine, (*str_t)[matchLine].line);
        }
        pAdvance[1] = COLOR_ERROR;
        pAdvance[4] = COLOR_ERROR;
        pMatch[1] = COLOR_ERROR;
        pMatch[4] = COLOR_ERROR;
        refresh_idaview_anyway();
      }
    }
    else
    {
      // do nothing yet
    }

    break;
  }
  case hxe_double_click:
  {
    // 当前位置发生改变
    vdui_t *vu = va_arg(va, vdui_t*);
    cfuncptr_t* pFun = &vu->cfunc;
    ctree_maturity_t eLevel = (*pFun)->maturity;
    if (eLevel != CMAT_FINAL)
    {
      msg("HightLight: ctree maturity is not CMAT_FINAL!\n");
      msg("ctree_maturity_t level is %d\n", eLevel);
      return 0;
    }
    if (!vu->visible())
    {
      msg("HightLight: pseudocode window is invisible!\n");
      return 0;
    }
    if (!vu->refresh_cpos(USE_KEYBOARD))
    {
      return 0;
    }

    ctext_position_t cPos = vu->cpos;
    int yPos = cPos.lnnum;
    strvec_t* str_t = (strvec_t*)&(*pFun)->get_pseudocode();

    qstring strBuf;
    tag_remove(&strBuf, (*str_t)[yPos].line.c_str(), MAX_NUMBUF);
    strBuf = removeComment(strBuf);

    /*
    if (strBuf.find("//") > 0)
    {
      strBuf = strBuf.substr(0, strBuf.find("//"));
    }
    */

    int blockBegin = findEx(strBuf, '{');
    int blockEnd = findEx(strBuf, '}');

    if (blockBegin >= 0 || blockEnd >= 0)
    {
      pTempHlight->restore_color(str_t);

      char findChar;
      int findIndex;
      int findPos;

      if (blockEnd >= 0)
      {
        findChar = '{';
        findIndex = -1;
        findPos = blockEnd;
      }
      else {
        findChar = '}';
        findIndex = 1;
        findPos = blockBegin;
      }

      (*str_t)[yPos].bgcolor = pTempHlight->get_hcolor();
      //
      int j = yPos + findIndex;
      int max = (*str_t).size();

      // 循环行
      while (j >= 0 && j < max)
      {
        qstring outStr;
        int len;

        tag_remove(&outStr, (*str_t)[j].line.c_str(), MAX_NUMBUF);

        /*
        int findLen = outStr.find("//");
        if (findLen > 0)
        {
          outStr = outStr.substr(0, findLen);
        }
        */

        outStr = removeComment(outStr);
        len = outStr.size();

        if (findPos >= 0 && findPos < len) {
          // 存在findChar, 并且不在字符串中
          if (outStr[findPos] == findChar && checkPosInString(outStr, findPos) == false) {
            (*str_t)[j].bgcolor = pTempHlight->get_hcolor();
            break;
          }
        }

        (*str_t)[j].bgcolor = pTempHlight->get_bcolor();

        j += findIndex;
      }
      if (findIndex > 0)
      {
        pTempHlight->set_begin_end(yPos, j);
      }
      else
      {
        pTempHlight->set_begin_end(j, yPos);
      }
      pTempHlight->set_hlight(true);
      pTempHlight->set_jLine(yPos);
      //vu.refresh_ctext(true);
      refresh_idaview_anyway();
    }
    else
    {
      if (pTempHlight->isHLight())
      {
        pTempHlight->restore_color(str_t);
        pTempHlight->clear();
        refresh_idaview_anyway();
      }
    }
    break;
  }
  case hxe_keyboard:
  {
    vdui_t &vu = *va_arg(va, vdui_t*);
    int keyCode = va_arg(va, int);
    int shift_state = va_arg(va, int);
    if (keyCode == 'j' || keyCode == 'J')
    {
      int x = 0;
      int y = 0;
      place_t *pLace = get_custom_viewer_place(vu.ct, false, &x, &y);
      simpleline_place_t *simPlace = (simpleline_place_t *)pLace->clone();
      simPlace->n = pTempHlight->get_jmpline();
      if (simPlace->n != -1)
      {
        jumpto(vu.ct, (place_t *)simPlace, x, y);
        if (pTempHlight->isHLight())
        {
          pTempHlight->set_jLine(simPlace->n);
        }
      }

    }
    break;
  }
  case hxe_right_click:
  {
    vdui_t &vu = *va_arg(va, vdui_t*);

    break;
  }
  case hxe_populating_popup:
  {
    TWidget &form = *va_arg(va, TWidget*);
    TPopupMenu &popup_handle = *va_arg(va, TPopupMenu*);
    vdui_t &vu = *va_arg(va, vdui_t*);
    break;
  }

  ///< Populating popup menu. We can add menu items now.
  ///< TForm *form
  ///< TPopupMenu *popup_handle
  ///< vdui_t *vu
  }
  return 0;
}

int idaapi init(void)
{
  msg("HightLight init begin \n");
  if (!init_hexrays_plugin())
  {
    return PLUGIN_SKIP;
  }

  // 开始添加hook
  if (pHLight == NULL)
  {
    pHLight = new HightLight();
    if (pHLight == NULL)
    {
      msg("there is no enough memory \n");
      return PLUGIN_SKIP;
    }
  }
  bgcolor_t hColor = DEFAULT_HCOLOR;
  bgcolor_t bColor = DEFAULT_BCOLOR;
  GetConfColor(hColor, bColor);
  pHLight->set_bcolor(bColor);
  pHLight->set_hcolor(hColor);
  if (!install_hexrays_callback(myhexrays_cb_t, pHLight))
  {
    if (pHLight)
    {
      delete pHLight;
      pHLight = NULL;
    }
    msg("install_hexrays_callback fail\n");
    return PLUGIN_SKIP;
  }

  msg("HeyRays version %s has been detected \n", get_hexrays_version());
  msg("**********HightLight tool************************\n");
  msg("**********Author: SpBird data: 2017-09-10**********\n");
  return PLUGIN_KEEP;
}

void idaapi term()
{
  // exit code
  // 结束hook
  remove_hexrays_callback(myhexrays_cb_t, NULL);
  if (pHLight)
  {
    delete pHLight;
    pHLight = NULL;
  }
  return;
}

bool idaapi run(size_t arg)
{
  unsigned short bRem = 1;
  bgcolor_t headC = DEFAULT_HCOLOR;
  bgcolor_t bodyC = DEFAULT_BCOLOR;

  GetConfColor(headC, bodyC);
  const char strFormat[] =
    "STARTITEM 0\n"
    "Options\n"
    "< # store the color #"
    "remember :C:1:::>>\n"
    "<~H~eadColor :K:16:32::>\n"
    "<~B~odyColor :K:16:32::>\n";
  if (!ask_form(strFormat, &bRem, &headC, &bodyC))
  {
    msg("--------   ");
  }
  pHLight->set_bcolor(bodyC);
  pHLight->set_hcolor(headC);
  if (bRem)
  {
    qstring headColor;
    qstring bodyColor;
    headColor.sprnt("0x%x", headC);
    bodyColor.sprnt("0x%x", bodyC);
    SetConfColor(headColor, bodyColor);
  }

  return true;
}

const char comment[] = "HightLight Plugin";
const char help[] = "Hotkey to set colors is Alt-5;if error occure, contact to SpBird";             ///< Multiline help about the plugin
const char wanted_name[] = "HightLight";      ///< The preferred short name of the plugin
const char wanted_hotkey[] = "Alt-3";    ///< The preferred hotkey to run the plugin

// init the export struct for ida.exe
plugin_t PLUGIN
{
  IDP_INTERFACE_VERSION,
  0,
  init,
  term,
  run,
  comment,
  help,
  wanted_name,
  wanted_hotkey
};
