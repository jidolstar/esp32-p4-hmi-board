---
name: use-relative-paths
description: Use portable repository paths in project documents, development plans, and code comments.
---

# 저장소 경로 표기

- 문서에서 저장소 파일을 설명할 때 `hosts/hmi/main.cpp`, `_plan/YYYY-MM/`처럼 저장소 루트 기준 경로를 쓴다.
- Markdown 파일 링크의 대상은 링크가 들어 있는 문서를 기준으로 계산한다. 예를 들어 `.agents/skills/<name>/SKILL.md`에서 루트 README로 연결할 때는 `../../../README.md`를 사용한다.
- 도구가 절대 경로를 요구하거나 채팅에서 클릭 가능한 로컬 파일 링크를 만들 때는 그 환경의 경로 규칙을 따른다.
