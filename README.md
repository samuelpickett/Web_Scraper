# Wikipedia C++ scraper

Fetch one English Wikipedia article by title and save its headings and paragraphs as JSON. Lead paragraphs are stored separately. Each section stores paragraphs directly under its heading and nests lower-level headings in `subsections`. Tables, lists, infoboxes, captions, navigation, and reference markers are excluded.

## Build on Windows with MSYS2 UCRT64

In the **MSYS2 UCRT64 terminal**, install the compiler and libraries:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-gumbo-parser mingw-w64-ucrt-x86_64-nlohmann-json
```

Make sure `C:\msys64\ucrt64\bin` is on `PATH`. From this folder:

```powershell
.\build.ps1 -Test
```

For VS Code, open the `Web_Scraper` folder as the workspace. `.vscode/c_cpp_properties.json` points IntelliSense at the same MSYS2 UCRT64 compiler and headers used by the build. If MSYS2 is installed elsewhere, update its paths in that file.

## Run

```powershell
.\web_scraper.exe
```

The window asks for a Wikipedia page title, a file name, and your contact email or website. Labels show examples for each field. Enter a file name such as `ada_lovelace`; the program adds `.json` automatically and saves it in `Scrapped_Pages` beside the executable. You can also enter a name ending in `.json`. The window stays open for another scrape and the program ends when you close it.

[Wikimedia asks automated clients](https://foundation.wikimedia.org/wiki/Policy:Wikimedia_Foundation_User-Agent_Policy/en) to identify themselves with a descriptive User-Agent and contact information. The program requests only one article at a time through its [HTML endpoint](https://www.mediawiki.org/wiki/API:REST_API/Reference/en#Get_HTML). It retries transient network and server errors twice, but reports missing pages and rate limits immediately in the window.

The output has this shape:

```json
{
  "title": "Example",
  "source_url": "https://en.wikipedia.org/wiki/Example",
  "lead_paragraphs": ["Introduction..."],
  "sections": [
    {
      "heading": "History",
      "paragraphs": ["Text directly under History..."],
      "subsections": [
        {"heading": "Early years", "paragraphs": ["Text..."], "subsections": []}
      ]
    }
  ]
}
```

The program writes the JSON to a temporary file before replacing the output file. If fetching, parsing, or writing fails, it shows an error and does not save a partial JSON result. A page can contain headings with empty `paragraphs` when that section contains only lists or references.
