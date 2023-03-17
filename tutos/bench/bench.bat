@echo off 

set prefix= bench
:: prefix="vega64"
:: prefix="geforce1070"
:: prefix="geforce1080ti"
:: prefix="geforce2080"
:: prefix="geforce3080"
:: prefix="ryzen5500U"
md %prefix%

:: transform
set triangles= 65536 262144 1048576 4194304

(for %%n in (%triangles%) do (
echo bench triangles %%n
REM ~ echo %prefix%/triangles-%%n.txt

bin\benchv3 --frames 1000 --rotation true --triangles %%n -o %prefix%/triangles-%%n.txt

))

:: fragments
set lights= 1 8 16 32

(for %%n in (%lights%) do (
echo bench lights %%n
REM ~ echo %prefix%/lights-%%n.txt

bin\benchv3 --frames 1000 --rotation true --lights %%n -o %prefix%/lights-%%n.txt

))

:: rasterization
set sizes= 4 8 16 32 64 128 256 512 1024

(for %%n in (%sizes%) do (
echo bench grid %%n
REM ~ echo %prefix%/grid-%%n.txt

bin\benchv3 --frames 2000 --rotation true --size %%n -o %prefix%/grid-%%n.txt

))