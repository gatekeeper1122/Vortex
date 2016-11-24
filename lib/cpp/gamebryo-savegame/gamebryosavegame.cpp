#include "gamebryosavegame.h"
#include <stdexcept>
#include <vector>
#include <ctime>
#include <nbind/nbind.h>

uint32_t windowsTicksToEpoch(int64_t windowsTicks)
{
  // windows tick is in 100ns
  static const int64_t WINDOWS_TICK = 10000000;
  // windows epoch is 1601-01-01T00:00:00Z which is this many seconds before the unix epoch
  static const int64_t SEC_TO_UNIX_EPOCH = 11644473600LL;

  return static_cast<uint32_t>(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}


GamebryoSaveGame::GamebryoSaveGame(const std::string &fileName)
 : m_FileName(fileName)
  //m_CreationTime(QFileInfo(file).lastModified()),
{
  FileWrapper file(this);

  for (auto hdr : {
    std::make_pair("TES4SAVEGAME", &GamebryoSaveGame::readOblivion),
    std::make_pair("TESV_SAVEGAME", &GamebryoSaveGame::readSkyrim),
    std::make_pair("FO3SAVEGAME", &GamebryoSaveGame::readFO3),
    std::make_pair("FO4_SAVEGAME", &GamebryoSaveGame::readFO4)
    }) {
      if (file.header(hdr.first)) {
        (this->*hdr.second)(file);
      }
  }
}

GamebryoSaveGame::~GamebryoSaveGame()
{
}

// don't want no dependency on windows header
struct WINSYSTEMTIME {
  uint16_t wYear;
  uint16_t wMonth;
  uint16_t wDayOfWeek;
  uint16_t wDay;
  uint16_t wHour;
  uint16_t wMinute;
  uint16_t wSecond;
  uint16_t wMilliseconds;
};

void GamebryoSaveGame::readOblivion(GamebryoSaveGame::FileWrapper &file)
{
  file.setBZString(true);

  file.skip<unsigned char>(); //Major version
  file.skip<unsigned char>(); //Minor version

  file.skip<WINSYSTEMTIME>();  // exe last modified (!)

  file.skip<unsigned long>(); //Header version
  file.skip<unsigned long>(); //Header size

  file.read(m_SaveNumber);

  file.read(m_PCName);
  file.read(m_PCLevel);
  file.read(m_PCLocation);

  file.skip<float>(); //game days
  file.skip<unsigned long>(); //game ticks

  WINSYSTEMTIME winTime;
  file.read(winTime);
  tm timeStruct;
  timeStruct.tm_year = winTime.wYear - 1900;
  timeStruct.tm_mon = winTime.wMonth - 1;
  timeStruct.tm_mday = winTime.wDay;
  timeStruct.tm_hour = winTime.wHour;
  timeStruct.tm_min = winTime.wMinute;
  timeStruct.tm_sec = winTime.wSecond; 
  m_CreationTime = mktime(&timeStruct);

  //Note that screenshot size, width, height and data are apparently the same
  //structure
  file.skip<unsigned long>(); //Screenshot size.

  file.readImage();

  file.readPlugins();
}

void GamebryoSaveGame::readSkyrim(GamebryoSaveGame::FileWrapper &file)
{
  file.skip<unsigned long>(); // header size
  file.skip<unsigned long>(); // header version
  file.read(m_SaveNumber);

  file.read(m_PCName);

  unsigned long temp;
  file.read(temp);
  m_PCLevel = static_cast<unsigned short>(temp);

  file.read(m_PCLocation);

  std::string timeOfDay;
  file.read(timeOfDay);

  std::string race;
  file.read(race); // race name (i.e. BretonRace)

  file.skip<unsigned short>(); // Player gender (0 = male)
  file.skip<float>(2); // experience gathered, experience required

  uint64_t ftime;
  file.read(ftime);
  m_CreationTime = windowsTicksToEpoch(ftime);

  file.readImage();

  file.skip<unsigned char>(); // form version
  file.skip<unsigned long>(); // plugin info size

  file.readPlugins();
}

void GamebryoSaveGame::readFO3(GamebryoSaveGame::FileWrapper &file)
{
  file.skip<unsigned long>(); //Save header size

  file.skip<unsigned long>(); //File version? always 0x30
  file.skip<unsigned char>(); //Delimiter

  // New Vegas has the same extension, file header, and version (if the previous field was, in fact,
  // a version field), but it has a string field here which FO3 doesn't have 

  uint64_t pos = file.tell();
  int fieldSize = 0;
  for (unsigned char ignore = 0; ignore != 0x7c; ++fieldSize) {
    file.read(ignore); // unknown
  }

  if (fieldSize == 5) {
    // if the field was only 4 bytes, it was a FO3 save after all so seek back since we need the
    // content of that field
    file.seek(pos);
  }

  file.setHasFieldMarkers(true);

  unsigned long width;
  file.read(width);

  unsigned long height;
  file.read(height);

  file.read(m_SaveNumber);

  file.read(m_PCName);

  std::string unknown;
  file.read(unknown);

  long level;
  file.read(level);
  m_PCLevel = level;

  file.read(m_PCLocation);

  std::string playtime;
  file.read(playtime);

  file.readImage(width, height, 256);

  file.skip<char>(5); // unknown byte, size of plugin data

  file.readPlugins();
}

void GamebryoSaveGame::readFO4(GamebryoSaveGame::FileWrapper &file)
{
  file.skip<uint32_t>(); // header size
  file.skip<uint32_t>(); // header version
  file.read(m_SaveNumber);

  file.read(m_PCName);

  uint32_t temp;
  file.read(temp);
  m_PCLevel = static_cast<uint16_t>(temp);
  file.read(m_PCLocation);

  std::string ignore;
  file.read(ignore);   // playtime as ascii hh.mm.ss
  file.read(ignore);   // race name (i.e. BretonRace)

  file.skip<uint16_t>(); // Player gender (0 = male)
  file.skip<float>(2);         // experience gathered, experience required

  uint64_t ftime;
  file.read(ftime);
  m_CreationTime = windowsTicksToEpoch(ftime);
  file.readImage(384, true);

  file.skip<uint8_t>(); // form version
  file.read(ignore);          // game version
  file.skip<uint32_t>(); // plugin info size

  file.readPlugins();
}

GamebryoSaveGame::FileWrapper::FileWrapper(GamebryoSaveGame *game)
 : m_Game(game),
  m_File(game->m_FileName, std::ios::in | std::ios::binary),
  m_HasFieldMarkers(false),
  m_BZString(false)
{
  if (!m_File.is_open()) {
    throw std::runtime_error(fmt::format("failed to open {}", game->m_FileName));
  }
}

bool GamebryoSaveGame::FileWrapper::header(const char *expected)
{
  std::string foundId;
  foundId.resize(strlen(expected));
  m_File.seekg(0);
  m_File.read(&foundId[0], foundId.length());

  return foundId == expected;
}

void GamebryoSaveGame::FileWrapper::setHasFieldMarkers(bool state)
{
  m_HasFieldMarkers = state;
}

void GamebryoSaveGame::FileWrapper::setBZString(bool state)
{
  m_BZString = state;
}

template <> void GamebryoSaveGame::FileWrapper::read(std::string &value)
{
  unsigned short length;
  if (m_BZString) {
    unsigned char len;
    read(len);
    length = len;
  } else {
    read(length);
  }
  std::string buffer;
  buffer.resize(length);
  read(&buffer[0], length);

  if (m_BZString) {
    buffer.resize(buffer.length() - 1);
  }

  if (m_HasFieldMarkers) {
    skip<char>();
  }

  // TODO: Need to convert from latin1 to utf8!
  value = buffer;
}

void GamebryoSaveGame::FileWrapper::read(void *buff, std::size_t length)
{
  if (!m_File.read(static_cast<char *>(buff), length)) {
    throw std::runtime_error(fmt::format("unexpected end of file at {} (read of {} bytes)", m_File.tellg(), length).c_str());
  }
}

void GamebryoSaveGame::FileWrapper::readImage(int scale, bool alpha)
{
  unsigned long width;
  read(width);
  unsigned long height;
  read(height);
  readImage(width, height, scale, alpha);
}

void GamebryoSaveGame::FileWrapper::readImage(unsigned long width, unsigned long height, int scale, bool alpha)
{
  m_Game->m_ScreenshotDim = Dimensions(width, height);

  int bpp = alpha ? 4 : 3;

  int bytes = width * height * bpp;

  std::vector<uint8_t> buffer;
  buffer.resize(bytes);
  read(&buffer[0], bytes);

  if (alpha) {
    // no postprocessing necessary
    m_Game->m_Screenshot = std::move(buffer);
  } else {
    // begin scary
    std::vector<uint8_t> rgba;
    rgba.resize(width * height * 4);
    uint8_t *in = &buffer[0];
    uint8_t *out = &rgba[0];
    uint8_t *end = in + bytes;
    for (; in < end; in += 3, out += 4) {
      memcpy(out, in, 3);
      out[3] = 0xFF;
    }
    // end scary

    m_Game->m_Screenshot = std::move(rgba);
  }
}

void GamebryoSaveGame::FileWrapper::readPlugins()
{
  unsigned char count;
  read(count);
  for (std::size_t i = 0; i < count; ++i) {
    std::string name;
    read(name);
    m_Game->m_Plugins.push_back(name);
  }
}