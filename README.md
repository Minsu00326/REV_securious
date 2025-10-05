# 📜 2025 연합 CTF REV 문제 - Securious

# Tic-Tac-Toe

<aside>

평범해 보이는 이 틱택토 게임 뒤에는 수학을 광적으로 좋아하는 관리자가 숨겨둔 비밀이 있다고 합니다.
단순한 3×3 보드판처럼 보이는 이 게임 속에서 숨겨진 규칙과 값들을 찾아 그가 남긴 비밀을 찾아보세요!

</aside>

![틱택토 예시](images/tictactoe.png)

# 문제 설명
## 빌드 및 난독화 방식

<aside>
코드에 명시된 어노테이션과 난독화 패턴은 주로 OLLVM (Obfuscator-LLVM) 프레임워크를 적용
</aside>

| 어노테이션 | OLLVM 기법 | 난독화 효과 |
| :---: | :--- | :--- |
| **`fla`** | 제어 흐름 평탄화 (Control-Flow Flattening) | 함수의 제어 흐름을 복잡하게 만들어 디컴파일러의 결과를 읽기 어렵게 함. |
| **`bcf`** | 불투명 술어 (Opaque Predicates) | 항상 참/거짓으로 결정되는 조건문을 삽입하여, 정적 분석(코드 흐름 추적)을 오도함. |
| **`sub`** | 함수 인라인 방지 (Function Substitution/Inlining Avoidance) | 컴파일러의 최적화(인라이닝)를 방해하여, 독립된 함수 호출 그래프를 유지함. |