#pragma once
#include <glm/glm.hpp>
#include <string>

#include "../../Canis/External/entt.hpp"

#include "../../Canis/ECS/Components/RectTransformComponent.hpp"
#include "../../Canis/ECS/Components/TextComponent.hpp"

class Wallet
{
private:
    //[Signal] public delegate void CashUpdated(int delta, int currentCash);
    //[Signal] public delegate void Earned(int amount);
    //[Signal] public delegate void Payed();
    //[Signal] public delegate void OutOfCash();

    int cash = 0;
public:
    entt::registry *refRegistry;
    entt::entity walletText;

    void Init()
    {

    }

    void UpdateUI()
    {
        auto [wallet_rect, wallet_text] = refRegistry->get<RectTransformComponent, TextComponent>(walletText);
						
		*wallet_text.text = "Gold : " + std::to_string(cash);
    }

    int GetCash()
    {
        return cash;
    }

    void SetCash(int _amount)
    {
        cash = _amount;

        UpdateUI();
    }

    bool ICanAfford(int _amount)
    {
        if(0 > (cash - _amount))
        {
            return false;
        }

        return true;
    }

    bool Pay(int _amount)
    {
        if (cash <= 0)
            return false;

        cash -= _amount;

        UpdateUI();
        
        if (cash <= 0)
        {
            //EmitSignal(nameof(OutOfCash));
            return false;
        }
        else
        {
            //EmitSignal(nameof(Payed));
            return true;
        }
        
        //EmitSignal(nameof(CashUpdated), -_amount, cash);
    }

    void Earn(int _amount) //, glm::vec3 _location)
    {
        if (cash < 0)
            return;

        cash += _amount;

        UpdateUI();

        //EmitSignal(nameof(Earned), _amount);
        //EmitSignal(nameof(CashUpdated), _amount, cash);
    }

};