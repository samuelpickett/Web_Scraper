param([switch]$Test)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    throw 'g++ was not found. Install the MSYS2 UCRT64 toolchain first.'
}
if (-not (Get-Command pkg-config -ErrorAction SilentlyContinue)) {
    throw 'pkg-config was not found. Install the MSYS2 UCRT64 toolchain first.'
}

$dependencyFlags = ((pkg-config --cflags --libs libcurl gumbo) -join ' ') -split '\s+'
if ($LASTEXITCODE -ne 0) {
    throw 'libcurl or Gumbo is missing. Install the packages listed in README.md.'
}

$commonFlags = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic')
& g++ @commonFlags -municode -mwindows src/main.cpp src/article_parser.cpp src/http_client.cpp src/article_output.cpp -o web_scraper.exe @dependencyFlags
if ($LASTEXITCODE -ne 0) {
    throw 'Building the scraper failed.'
}
Write-Host 'Built web_scraper.exe'

if ($Test) {
    & g++ @commonFlags tests/parser_tests.cpp src/article_parser.cpp -o parser_tests.exe @dependencyFlags
    if ($LASTEXITCODE -ne 0) {
        throw 'Building the parser tests failed.'
    }
    & .\parser_tests.exe
    if ($LASTEXITCODE -ne 0) {
        throw 'Parser tests failed.'
    }
    Write-Host 'Parser tests passed.'

    & g++ @commonFlags tests/output_tests.cpp src/article_output.cpp src/http_client.cpp -o output_tests.exe @dependencyFlags
    if ($LASTEXITCODE -ne 0) {
        throw 'Building the output tests failed.'
    }
    & .\output_tests.exe
    if ($LASTEXITCODE -ne 0) {
        throw 'Output tests failed.'
    }
    Write-Host 'Output tests passed.'
}
