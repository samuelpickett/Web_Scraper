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

## Run

```powershell
.\web_scraper.exe "Ada Lovelace" "ada_lovelace.json" "you@example.com"
```

Replace the last argument with your own contact email or website. [Wikimedia asks automated clients](https://foundation.wikimedia.org/wiki/Policy:Wikimedia_Foundation_User-Agent_Policy/en) to identify themselves with a descriptive User-Agent and contact information. The program requests only one article at a time through its [HTML endpoint](https://www.mediawiki.org/wiki/API:REST_API/Reference/en#Get_HTML). It retries transient network and server errors twice, but reports missing pages and rate limits immediately.

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

The program writes the JSON to a temporary file before replacing the output file. If fetching, parsing, or writing fails, it prints an error and does not save a partial JSON result. A page can contain headings with empty `paragraphs` when that section contains only lists or references.
