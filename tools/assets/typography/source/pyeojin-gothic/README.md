# Pyeojin Gothic source fonts

이 폴더의 Regular·Bold TTF는 고정 UI용 LVGL bitmap font를 생성하기 위한 원본이다. TTF 자체를 펌웨어에 포함하지 않는다. 202607 프로젝트의 NanumGothic 원본·생성물은 사용하지 않는다.

- 제작자 저장소: https://github.com/Jihwan-Suh/PyeojinGothic
- 고정 리비전: `0f9af0f3365037c1127a2e96828df3b2b9bd0c85`
- 원본 위치: `fonts/static/ttf/`
- 라이선스: 동봉한 `LICENSE.txt` (SIL Open Font License 1.1)

| 파일 | OpenType weight | SHA-256 | 원본 Git blob SHA-1 |
|---|---:|---|---|
| `PyeojinGothic-Regular.ttf` | 400 | `2f2a6c4c02a7e83feef8c192c337da955932352ef2ccbb963c33677bbd1fcefb` | `fe672a75cc8ea07ad0e636de55e04cfbfabc961d` |
| `PyeojinGothic-Bold.ttf` | 700 | `55b261c3c81c9b84f50ef58a8e076bfc6dc88c163305f1a7ea2807939ebc4624` | `713a826cf737b5754649c751bfa1adc200cc33f6` |
| `LICENSE.txt` | — | `73bb743148ffd2f586e7217c36c125e777824cfdb54a3cc22beef57890483205` | `675ff21d28f86ec7662b55b3a6d7146c3bf311d6` |

`git hash-object --no-filters <파일>`로 원본 blob SHA-1을 검사한다. Windows의 `core.autocrlf=true`에서는 일반 `git hash-object`가 텍스트 라이선스 파일에 줄바꿈 필터를 적용할 수 있다. `Get-FileHash -Algorithm SHA256 <파일>`로 내려받은 파일 바이트를 확인한다.
