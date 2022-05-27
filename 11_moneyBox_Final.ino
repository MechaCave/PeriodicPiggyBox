/*
 * 적금형 저금통
 * 
 * 2022.05.11
 * 
 * [포트설정]
 * 1. 동전확인 센서 : 작은10원(D8), 50원(D9), 100원(D10), 500원(D11) - 적외선 센서
 * 2. LCD : I2C통신방식, (4개선 VCC, GND, A4, A5)
 * 3. 버저 : D7
 * 4. 서보모터 : D6
 * 5. 로터리 스위치 : CLK(D2), DATA(D3), Switch(D4), VCC, GND
 * 
 */


#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EncoderButton.h>
#include <pitches.h>
#include <Servo.h>
#include <MsTimer2.h>

// 핀 정의
// 동전 확인용 센서 연결 핀
#define c10 8
#define c50 9
#define c100 10
#define c500 11

#define pServo 6    // 서보모터
#define pBUZZ 7     // 부저

#define pLED A0     // 알람용 빨간 LED

// 현재 상태 설정
#define MENU 0    // 메뉴를 고르는 상황인 경우
#define MODIFY 1  // 값을 수정하는 상황인 경우

// 메뉴 설정
#define CURRENT 1   // 현재 금액 - 수정 가능
#define TARGET 2    // 목표 금액 - 수정 가능
#define DDAY 3      // 다음 적립일 (년/월/일) - 수정 불가 (이전 적립일로부터 시작, 저축한 시기랑 관계 없음)
#define INTERVAL 4  // 적립 간격(일) - 수정 가능
#define DEADLINE 5  // 최종 목표일 (기한 정하기) - 수정 가능 (D-000으로 표시)
#define SUCCESS 6   // 성공했을 때의 메시지
#define RESET 7     // 처음부터 다시 시작
#define SAVE_ALARM 8  // 적립일이 됐을 떄 알람 울림
#define TIMEISUP 9  // 최종목표일이 자났을 때

// 서보모터 각도 설정
#define OPEN 0
#define CLOSE 80

// LCD 설정
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// 엔코더 설정
EncoderButton eBtn(2,3,4);

// 서보모터 설정
Servo servoDoor;

// 동전이 들어왔는지 확인
bool is10, is50, is100, is500;

// 목표금액(1000원 단위) - 기본 1만원 / 최대 10만원
long targetAmount = 0;
long maxTargetAmount = 0;

// 현재 금액이 목표 금액을 넘었을 때 true
bool alreadyFull = false;

// 현재금액(10원 단위)
long currentAmount = 0;

// 적립간격 - 기본 7일 / 최대 30일
int intervalDays = 0;
int maxIntervalDays = 0;

// 적립일 까지 남은 일 수
int daysLeft = 0;

// 지난 적립일 저장
int oldDaysLeft = 0;

// 적립일이 되면 True가 되는 알람용
bool isAlarm = false;
// 성실도 판단을 위한 알람 카운트
int cntAlarm = 0;

// 적립기한 (데드라인), 최대 적립기한
int deadline = 0;
int maxDeadline = 0; 
int oldDeadline = 0;

// 데드라인까지 남은기한
int deadlineLeft = 0;

// 시연용으로 하루를 몇초로 잡을까. 
int trickDay = 3;

// 현재 각 동전의 개수
long n10 = 0;
long n50 = 0;
long n100 = 0;
long n500 = 0;

// 메뉴
int cMenu = CURRENT; // 최초 메뉴는 '현재 금액'
int oldMenu = 0;      // 이전 메뉴 저장

// 상태
int cState = MENU; // 최초 상태는 MENU가 보이는 상태

// 축하 멜로디
int melodyGOOD[] = {NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
int melodyALARM[] = {NOTE_B3, NOTE_C4, NOTE_B3, NOTE_C4, NOTE_B3, NOTE_C4, NOTE_B3, NOTE_C4};

// 음표 길이
int noteDurationsGOOD[] = {4, 8, 8, 4, 4, 4, 4, 4};
int noteDurationsALARM[] = {4, 8, 4, 8, 4, 8, 4, 8,};

/////////////////////////////////////////////////
//
// 필요한 함수 정의
//
////////////////////////////////////////////////

// 기본 변수 초기화 : 모든 초기화는 여기서
void initValues()
{
  // 일수 타이머 정지
  MsTimer2::stop();

  targetAmount = 5000;    // 목표금액 (천원단위)
  maxTargetAmount = 100000;  // 최대 목표금액

  currentAmount = 0;  // 현재 금액 

  alreadyFull = false; // 가득 차지 않았다고 설정

  intervalDays = 7;   // 적립 간격(일)
  maxIntervalDays = 30;   // 최대 적립간격

  daysLeft = intervalDays;  // 남은 일수는 적립일수

  deadline = 10;  // 적립 기한(일) : 00일 동안 목표한 금액을 넣어야 성공!!
  maxDeadline = 365;   // 최대 적립 기한(일)

  cntAlarm = 0; // 알람 개수 초기화

  // 현재 각 동전의 개수
  n10 = 0;
  n50 = 0;
  n100 = 0;
  n500 = 0;

  cMenu = CURRENT;
  cState = MENU;

  // 문 닫기
  servoDoor.write(CLOSE);
  delay(3000);
  
  // LED 끄기
  digitalWrite(pLED, LOW);

  // 일수 타이머 시작
  MsTimer2::start();
}

// 값을 변경하는 함수
long changeAmount(long _amount, long _maxAmount, int _gap)
{
  _amount = _amount + _gap;

  // 설정한 0보다 작아지거나 최대값을 넘지 않도록 한다.
  if(_amount > _maxAmount)  _amount = _maxAmount;
  else if(_amount < 0) _amount = 0;

  // 바뀐 값을 돌려준다.
  return _amount;
}

// LCD에 변한 값을 출력해주는 함수
void updateAmount()
{
  // 알람상태에서 입금이 되면 상태를 현재값으로 변경
  if(cMenu == SAVE_ALARM)
  {
    cMenu = CURRENT;
  }

  switch(cMenu)
  {
    case CURRENT:
                  // 알람상태였다가 현재금액이 바뀌면 알람을 끈다.
                  if(isAlarm) 
                  {
                    isAlarm = false;
                    digitalWrite(pLED, LOW);
                  }
                  lcd.setCursor(0,1);
                  lcd.print("         ");
                  lcd.setCursor(0,1);
                  lcd.print(currentAmount);
      break;

      case TARGET:
                  lcd.setCursor(0,1);
                  lcd.print("         ");
                  lcd.setCursor(0,1);
                  lcd.print(targetAmount);
      break;

      case DDAY:     
                  lcd.setCursor(0,1);
                  lcd.print("   ");
                  lcd.setCursor(0,1);
                  lcd.print(daysLeft);   

                  //daysLeft 저장
                  oldDaysLeft = daysLeft;          
      break;

      case INTERVAL:
                  lcd.setCursor(6,1);
                  lcd.print("   ");
                  lcd.setCursor(6,1);
                  lcd.print(intervalDays);
      break;

      case DEADLINE:
                  lcd.setCursor(3,1);
                  lcd.print("   ");
                  lcd.setCursor(3,1);
                  lcd.print(deadline);

                  // deadline 저장
                  oldDeadline = deadline;
      break;

    break;
  }
}

// 로터리 스위치를 눌렀을 때 실행되는 함수 선언
void eBtnClicked(EncoderButton& eBtn)
{  
  //Serial.println("CLICKED");
  
  // 메뉴가 리셋일 때 클릭하면, 모든 값을 기본값으로 셋팅
  if(cMenu == RESET)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Wait a second...");

    initValues();
    cMenu = CURRENT;
    changeDisplay();
  }
  // 알람이 떳을 때 클릭하면, 현재값 보여주기
  else if(cMenu == SAVE_ALARM)
  {
    cMenu = CURRENT;
    changeDisplay();
  }
  // 스위치를 클릭했을 경우, 상태를 MENU 또는 MODIFY로 바꾼다.    
  // 현재 메뉴가 다음 적립일을 보여주는 경우는 바꿀 수 없으므로 패스.
  else if(cMenu != DDAY)
  {
    if(cState == MENU)  
    {
      cState = MODIFY;
      Serial.print("MODIFY");
      lcd.setCursor(0,0);
      lcd.print("@");
    }
    else if(cState == MODIFY) 
    {
      cState = MENU;
      Serial.print("MENU");
      lcd.setCursor(0,0);
      lcd.print(cMenu);
    }
  }
}

// 로터리 스위치를 돌렸을 때 실행되는 함수 선언
void eBtnRolled(EncoderButton& eBtn)
{
  int _increment = eBtn.increment();

  // 리셋상태일 때 스위치 돌려도 아무일도 안 일어난다.
  if(cMenu == RESET)
  {
    return;
  }
  
  // 알람상태에서 스위치 돌리면 현재값 보여주기
  if(cMenu == SAVE_ALARM)
  {
    cMenu = CURRENT;
    changeDisplay();
  }

  if(cState == MENU)
  {
    cMenu = cMenu + _increment;
    if(cMenu < CURRENT) cMenu = DEADLINE;
    else if(cMenu > DEADLINE) cMenu = CURRENT;

    changeDisplay();
  }
  else if(cState == MODIFY)
  {
    switch(cMenu)
    {
      case CURRENT:
                  currentAmount = changeAmount(currentAmount, maxTargetAmount, _increment*10);                  
      break;

      case TARGET:
                  targetAmount = changeAmount(targetAmount, maxTargetAmount, _increment*1000);
      break;

      case DDAY:                  
      break;

      case INTERVAL:
            intervalDays = changeAmount(intervalDays, maxIntervalDays, _increment);
      break;

      case DEADLINE:
            deadline = changeAmount(deadline, maxDeadline, _increment);
      break;
    }

    // 변한 값 반영
    updateAmount();
  }
}

// 동전이 저금됐는지 확인하는 함수
void checkSaving()
{ 
  // 동전을 확인하는 방법
  // 센서 앞을 동전이 가로 막았다가 (센서값이 0)
  // 동전이 다시 없어지면 (센서값이 1) 동전이 있다고 판단한다.

  // 10원 짜리 동전이 들어왔는지 판단하기
  is10 = digitalRead(c10);
  if(is10 == 0)
  {    
    while(is10 == 0)
    {
      is10 = digitalRead(c10);   
    }
    n10 = n10 + 1;
    currentAmount = currentAmount + 10;
    //Serial.print("10원 저금\t"); Serial.print(n10); Serial.print("\t"); Serial.println(currentAmount);
    Serial.print("1");
    updateAmount();    
  }

  // 50원 짜리 동전이 들어왔는지 판단하기
  is50 = digitalRead(c50);
  if(is50 == 0)
  {    
    while(is50 == 0)
    {
      is50 = digitalRead(c50);   
    }
    n50 = n50 + 1;
    currentAmount = currentAmount + 50;
    //Serial.print("50원 저금\t"); Serial.print(n50); Serial.print("\t"); Serial.println(currentAmount);    
    Serial.print("2");
    updateAmount();    
  }
  
  // 100원 짜리 동전이 들어왔는지 판단하기
  is100 = digitalRead(c100);
  if(is100 == 0)
  {    
    while(is100 == 0)
    {
      is100 = digitalRead(c100);   
    }
    n100 = n100 + 1;
    currentAmount = currentAmount + 100;
    //Serial.print("100원 저금\t"); Serial.print(n100); Serial.print("\t"); Serial.println(currentAmount);    
    Serial.print("3");
    updateAmount();    
  }

  // 500원 짜리 동전이 들어왔는지 판단하기
  is500 = digitalRead(c500);
  if(is500 == 0)
  {    
    while(is500 == 0)
    {
      is500 = digitalRead(c500);   
    }
    n500 = n500 + 1;
    currentAmount = currentAmount + 500;
    //Serial.print("500원 저금\t"); Serial.print(n500); Serial.print("\t"); Serial.println(currentAmount);    
    Serial.print("4");
    updateAmount();    
  }
}

// LCD 화면 전환
void changeDisplay()
{
  //Serial.println(cMenu);
  // 현재메뉴를 지난메뉴에 저장
  oldMenu = cMenu;

  // LCD 화면 지우고
  lcd.clear();

  // 메뉴에 맞게 화면 표시하기
  switch(cMenu)
  {
    case CURRENT:
                  lcd.setCursor(0,0);
                  lcd.print("1-CURRENT AMOUNT");                  
                  updateAmount();
    break;

    case TARGET:
                  lcd.setCursor(0,0);
                  lcd.print("2-TARGET AMOUNT");
                  updateAmount();
    break;

    case DDAY:
                  lcd.setCursor(0,0);
                  lcd.print("3-NEXT SAVING");
                  lcd.setCursor(3,1);
                  lcd.print("days left");                  
                  updateAmount();                   
    break;

    case INTERVAL:
                  lcd.setCursor(0,0);
                  lcd.print("4-INTERVAL");
                  lcd.setCursor(0,1);
                  lcd.print("Every");
                  lcd.setCursor(9,1);
                  lcd.print("days");
                  updateAmount();
    break;

    case DEADLINE:
                  lcd.setCursor(0,0);
                  lcd.print("5-DEADLINE");
                  lcd.setCursor(0,1);
                  lcd.print("D- ");                  
                  updateAmount();
    break;

    case SUCCESS:    
                  // 축하 메시지                        
                  lcd.setCursor(0,0);
                  lcd.print("CONGRATURATION!");
                  lcd.setCursor(0,1);
                  lcd.print("YOU DID IT!");        

                  // 목표금액이 찼다고 설정
                  alreadyFull = true;          
                  
                  // 축하 멜로디 : 간단하게
                  playMelodySOSO();
                  
                  // 다시 저금 
                  cMenu = CURRENT;
    break;

    case RESET:    
                  lcd.setCursor(0,0);
                  lcd.print("SAVE AGAIN!");
                  lcd.setCursor(0,1);
                  lcd.print("CLICK TO RESTART");
                  
                  //todo: cMenu가 상관있는지 확인 : 스위치가 눌려서 새로 시작될 때 까지 기다린다.
                  // 리셋상태에서 휠 돌리면 아무동작 안하도록 설정.
                  while(cMenu != CURRENT)
                  {
                    eBtn.update();
                  }
    break;

    case SAVE_ALARM:    
                  // 1. 알람 메시지
                  lcd.setCursor(0,0);
                  lcd.print("SAVE NOW !!!");
                  lcd.setCursor(0,1);
                  lcd.print("INSERT COINs !!");
                  
                  //2. LED 켜기
                  digitalWrite(pLED, HIGH);
                  
                  // 3. 알람 멜로디
                  playMelodyALARM();       

                  // 상태가 MODIFY 였을 때, 알람이 울리면 되돌아갈 수 없으므로 상태를 MENU로 강제 변경
                  cState = MENU;

    break;

    case TIMEISUP:
                  // 0. 타이머 멈춤
                  MsTimer2::stop();

                  // 1. 알람 메시지
                  lcd.setCursor(0,0);
                  lcd.print("TIME IS UP !!!");
                  lcd.setCursor(0,1);
                  lcd.print("LET'S CHECK");

                  // 2. 멜로디 추가
                  playMelodyOPEN();
                  // 여기서 문 열어도 된다. 
                  // 3. 저금통 문 열기
                  servoDoor.write(OPEN);                  
                  delay(1000);

                  lcd.clear();                  

                  // 목표 금액에 어느정도를 달성했는지 확인
                  float ratio = ((currentAmount*1.0) / (targetAmount*1.0)) * 100.0;
                  // 소수점 첫째자리까지 남기기
                  ratio = floor(ratio*10) * 0.1;

                  // 목표달성률 표시
                  lcd.setCursor(0,0);
                  lcd.print((String)ratio+"\% achieved");
                    
                  // > 목표금액 초과 달성 : VERY GOOD                  
                  if(ratio >= 100)
                  {
                    lcd.setCursor(0,1);
                    lcd.print("VERY GOOD !!");

                    // 축하 멜로디 - 팡파레
                    playMelodyGOOD();
                                    
                  }
                  // > 목표금액 90% 이상 달성 : GOOD
                  else if(ratio >= 90)
                  {
                    lcd.setCursor(0,1);
                    lcd.print("GOOD JOB !!");

                    // 축하 멜로디 - 팡파레
                    playMelodyGOOD();
                  }                  
                  // > 목표금액 80% 이상 달성 : SOSO
                  else if(ratio >= 80)
                  {
                    lcd.setCursor(0,1);
                    lcd.print("SO SO ~");

                    // 평범한 멜로디
                    playMelodySOSO();
                    
                  }
                  // > 목표금액 50% 이상 달성 : BAD
                  else if(ratio >= 50)
                  {
                    lcd.setCursor(0,1);
                    lcd.print("BAD~");

                    // 띠로리 멜로디
                    playMelodyBAD();
                    
                  }
                  // > 목표금액 50% 미만 달성 : VERY BAD
                  else
                  {                    
                    lcd.setCursor(0,1);
                    lcd.print("VERY BAD !!");

                    // 띠로리 멜로디
                    playMelodyBAD();
                  }                                            
                    // 잠시 후에
                    delay(3000);
                    // 리셋
                    cMenu = RESET;
    break;
  }
}

void playMelodyOPEN()
{
  // 1000 나누기 음표길이 
  // 만약 4분음표면, 1000/4 = 250
  // 8분음표면, 1000/8 = 125
  
  tone(pBUZZ,NOTE_E6,125);  delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ,NOTE_G6,125);  delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ,NOTE_E7,125);  delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ,NOTE_C7,125);  delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ,NOTE_D7,125);  delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ,NOTE_G7,125);  delay(125*1.3); noTone(pBUZZ);
}

void playMelodyGOOD()
{
  tone(pBUZZ, NOTE_F6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 250);    delay(250*1.3); noTone(pBUZZ);
  
  tone(pBUZZ, NOTE_F6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_F6, 250);    delay(250*1.3); noTone(pBUZZ);

  tone(pBUZZ, NOTE_F6, 125);    delay(125*1.3); noTone(pBUZZ);

  delay(125);
  
  tone(pBUZZ, NOTE_G5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_B5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C6, 500);    delay(500*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G5, 500);    delay(500*1.3); noTone(pBUZZ);
  
  delay(250);
  
  tone(pBUZZ, NOTE_C6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_B5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_D6, 500);    delay(500*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A5, 500);    delay(500*1.3); noTone(pBUZZ);

  delay(250);

  tone(pBUZZ, NOTE_A5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_B5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_E6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_D6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_D6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C6, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_B5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_B5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C6, 250);    delay(250*1.3); noTone(pBUZZ);

  delay(250);
  
  tone(pBUZZ, NOTE_G5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_E5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C5, 250);    delay(250*1.3); noTone(pBUZZ);
  delay(250);
}

// 축하 멜로디
void playMelodySOSO()
{
  for (int thisNote = 0; thisNote < 8; thisNote++) 
  {
    int noteDuration = 1000 / noteDurationsGOOD[thisNote];
    tone(pBUZZ, melodyGOOD[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    noTone(pBUZZ);
  }
}

// 알람 멜로디
void playMelodyALARM()
{
  for (int thisNote = 0; thisNote < 8; thisNote++) 
  {
    int noteDuration = 1000 / noteDurationsALARM[thisNote];
    tone(pBUZZ, melodyALARM[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    noTone(pBUZZ);
  }
}
  
void playMelodyBAD()
{
  
  tone(pBUZZ, NOTE_C5, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G4, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G4, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A4, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G4, 250);    delay(250*1.3); noTone(pBUZZ);
  delay(250);
  
  tone(pBUZZ, NOTE_B4, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C5, 250);    delay(250*1.3); noTone(pBUZZ);
  delay(250);

  tone(pBUZZ, NOTE_C5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_D5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C5, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A4, 125);    delay(125*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_A4, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_G4, 250);    delay(250*1.3); noTone(pBUZZ);
  delay(250);
  
  tone(pBUZZ, NOTE_B4, 250);    delay(250*1.3); noTone(pBUZZ);
  tone(pBUZZ, NOTE_C5, 250);    delay(250*1.3); noTone(pBUZZ);
  delay(250);
}

// 타이머 인터럽트로 하루씩 증가하는 함수
void dayGone()
{
  // 알람이 울리고 하루가 지나도 아무런 변화(스위치 누르거나, 돌리거나, 입금되거나)가 없으면
  // 자동으로 현재 금액 메뉴로 변경
  if(cMenu == SAVE_ALARM)
  {
    //Serial.println((String)"timer:"+ daysLeft + "\t"+oldMenu);
    cMenu = CURRENT;
  }
  // 남은 일수를 하루씩 줄인다.
  daysLeft = daysLeft - 1;
  
  // 데드라인을 하루씩 줄인다.
  deadline = deadline - 1;

  if(deadline < 1)
  {
    // 종료 시킨다.
    cMenu = TIMEISUP;
  }

  // 남은 일수가 0이 되면 적립을 해야하는 날이 지난 것이므로 알림.
  if(daysLeft < 1)
  {
    isAlarm = true;
    // 적립일 리셋 (적립이 안되더라도 적립일은 리셋되어 다음 요일까지 카운트한다.)
    daysLeft = intervalDays;

    // 알람카운트 증가
    cntAlarm = cntAlarm + 1;       

     // 메뉴를 알람으로 변경
    cMenu = SAVE_ALARM;
  }
}

/////////////////////////////////////////////////
//
// setup() 과 loop()
//
////////////////////////////////////////////////

void setup()
{
  // 시리얼 모니터 연결, 속도 9600bps
  Serial.begin(9600);

  // 센서입력 핀 설정
  pinMode(c10, INPUT);
  pinMode(c50, INPUT);
  pinMode(c100, INPUT);
  pinMode(c500, INPUT);

  // 알람용 빨간 LED 설정
  pinMode(pLED, OUTPUT);
  
  // LCD 초기화
  lcd.init();
  lcd.backlight();  
  changeDisplay();

  // 로터리 스위치에 관련 함수 연결
  // 스위치가 눌렸을 경우 실행될 함수 연결
  eBtn.setClickHandler(eBtnClicked);
  // 스위치를 돌렸을 경우 실행될 함수 연결
  eBtn.setEncoderHandler(eBtnRolled);

  // 서보모터 연결
  servoDoor.attach(pServo);

  // 타이머 인터럽트 : D-Day 체크용, 하루를 trickDay초로 잡는다.
  MsTimer2::set(trickDay*1000, dayGone);

  // 모든 변수, 모터 초기화
  initValues();
}

void loop()
{
  // 동전 체크
  checkSaving();
  // 로터리 스위치 체크
  eBtn.update();

  // 메뉴가 바뀌면 화면 전환
  if(oldMenu != cMenu)
  {
    changeDisplay();
  }

  // 메뉴가 DDAY인 동안에 하루가 지나면 바뀐 날짜 보여주기
  if(cMenu == DDAY && oldDaysLeft != daysLeft)
  {
    changeDisplay();
  }

  // 메뉴가 DEADLINE인 동안에 하루가 지나면 바뀐 날짜 보여주기
  if(cMenu == DEADLINE && oldDeadline != deadline)
  {
    changeDisplay();
  }

  // 현재 금액이 목표 금액보다 크거나 같다면 성공 메시지 
  if((currentAmount >= targetAmount) && (alreadyFull == false))
  {
    // 메뉴를 성공으로 바꾼다.
    cMenu = SUCCESS;
    changeDisplay();
  }
}
