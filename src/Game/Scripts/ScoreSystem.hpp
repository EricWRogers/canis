#pragma once
#include <glm/glm.hpp>

class ScoreSystem
{
private:
    //[Signal] public delegate void ScoreUpdated(int totalScore, int delta);
    //public static ScoreSystem Instance { get; private set; }
    int Score = 0;
    int Multiplier = 1;

    int cash = 200;
public:
    entt::registry *refRegistry;
    entt::entity scoreText;

    void UpdateUI()
    {
        auto [score_rect, score_text] = refRegistry->get<RectTransformComponent, TextComponent>(scoreText);
						
		*score_text.text = "Score : " + std::to_string(Score);
    }

    int GetScore()
    {
        return Score;
    }

    int GetMultiplier()
    {
        return Multiplier;
    }

    void ClearScore()
    {
        Score = 0;
        Multiplier = 1;

        UpdateUI();
    }

    void AddPoints(int _amount) //, glm::vec3 _location)
    {
        Score += (_amount * Multiplier);

        UpdateUI();

        //EmitSignal(nameof(ScoreUpdated), Score, (_amount * Multiplier));
    }

};