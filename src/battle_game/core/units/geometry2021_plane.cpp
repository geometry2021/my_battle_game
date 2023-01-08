#include "geometry2021_plane.h"

#include "battle_game/core/bullets/bullets.h"
#include "battle_game/core/game_core.h"
#include "battle_game/graphics/graphics.h"

namespace battle_game::unit {

    namespace {
        uint32_t plane_body_model_index=0xffffffffu;
        uint32_t plane_turret_model_index=0xffffffffu;
    }  // namespace

    Plane::Plane(GameCore *game_core,uint32_t id,uint32_t player_id)
        : Unit(game_core,id,player_id) {
        if (!~plane_body_model_index) {
            auto mgr=AssetsManager::GetInstance();
            {
                /* Plane Body */
                plane_body_model_index=mgr->RegisterModel(
                    {{{-0.4f, 1.5f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
                     {{-0.4f, -1.5f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
                     {{0.4f, 1.5f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
                     {{0.4f, -1.5f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}},
                    {0, 1, 2, 1, 2, 3});
            }

            {
                /* Plane Turret */
                std::vector<ObjectVertex> turret_vertices;
                std::vector<uint32_t> turret_indices;
                const int precision=60;
                const float inv_precision=1.0f/float(precision);
                for (int i=0; i<precision; i++) {
                    auto theta=(float(i)+0.5f)*inv_precision;
                    theta*=glm::pi<float>()*2.0f;
                    auto sin_theta=std::sin(theta);
                    auto cos_theta=std::cos(theta);
                    turret_vertices.push_back({{sin_theta*0.2f, cos_theta*0.2f},
                                               {0.0f, 0.0f},
                                               {0.7f, 0.7f, 0.7f, 1.0f}});
                    turret_indices.push_back(i);
                    turret_indices.push_back((i+1)%precision);
                    turret_indices.push_back(precision);
                }
                turret_vertices.push_back(
                    {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
                turret_vertices.push_back(
                    {{-0.1f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
                turret_vertices.push_back(
                    {{0.1f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
                turret_vertices.push_back(
                    {{-0.1f, 1.2f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
                turret_vertices.push_back(
                    {{0.1f, 1.2f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
                turret_indices.push_back(precision+1+0);
                turret_indices.push_back(precision+1+1);
                turret_indices.push_back(precision+1+2);
                turret_indices.push_back(precision+1+1);
                turret_indices.push_back(precision+1+2);
                turret_indices.push_back(precision+1+3);
                plane_turret_model_index=
                    mgr->RegisterModel(turret_vertices,turret_indices);
            }
        }
    }

    void Plane::Render() {
        battle_game::SetTransformation(position_,rotation_);
        battle_game::SetTexture(0);
        battle_game::SetColor(game_core_->GetPlayerColor(player_id_));
        battle_game::DrawModel(plane_body_model_index);
        battle_game::SetRotation(turret_rotation_);
        battle_game::DrawModel(plane_turret_model_index);
    }

    void Plane::Update() {
        PlaneMove(3.0f,glm::radians(180.0f));
        TurretRotate();
        Fire();
    }

    void Plane::PlaneMove(float move_speed,float rotate_angular_speed) {
        auto player=game_core_->GetPlayer(player_id_);
        if (player) {
            auto &input_data=player->GetInputData();
            glm::vec2 offset{0.0f};
            offset.y+=1.0f;
            float speed=move_speed*GetSpeedScale();
            offset*=kSecondPerTick*speed;
            auto new_position=
                position_+glm::vec2{glm::rotate(glm::mat4{1.0f}, rotation_,
                                                  glm::vec3{0.0f, 0.0f, 1.0f})*
                                      glm::vec4{offset, 0.0f, 0.0f}};
            if (!game_core_->IsBlockedByObstacles(new_position)) {
                game_core_->PushEventMoveUnit(id_,new_position);
            }
            float rotation_offset=0.0f;
            if (input_data.key_down[GLFW_KEY_A]) {
                rotation_offset+=1.0f;
            }
            if (input_data.key_down[GLFW_KEY_D]) {
                rotation_offset-=1.0f;
            }
            rotation_offset*=kSecondPerTick*rotate_angular_speed*GetSpeedScale();
            game_core_->PushEventRotateUnit(id_,rotation_+rotation_offset);
        }
    }

    void Plane::TurretRotate() {
        auto player=game_core_->GetPlayer(player_id_);
        if (player) {
            auto &input_data=player->GetInputData();
            auto diff=input_data.mouse_cursor_position-position_;
            if (glm::length(diff)<1e-4) {
                turret_rotation_=rotation_;
            }
            turret_rotation_=std::atan2(diff.y,diff.x)-glm::radians(90.0f);
        }
    }

    void Plane::Fire() {
        if (fire_count_down_) {
            fire_count_down_--;
        }
        else {
            auto player=game_core_->GetPlayer(player_id_);
            if (player) {
                auto &input_data=player->GetInputData();
                if (input_data.mouse_button_down[GLFW_MOUSE_BUTTON_LEFT]) {
                    auto velocity=Rotate(glm::vec2{0.0f, 20.0f},turret_rotation_);
                    GenerateBullet<bullet::CannonBall>(
                        position_+Rotate({0.0f, 1.2f},turret_rotation_),
                        turret_rotation_,GetDamageScale(),velocity);
                    fire_count_down_=kTickPerSecond;  // Fire interval 1 second.
                }
            }
        }
    }

    bool Plane::IsHit(glm::vec2 position) const {
        position=WorldToLocal(position);
        return position.x>-0.8f&&position.x < 0.8f&&position.y > -1.0f&&
            position.y<1.0f;
    }

    const char *Plane::UnitName() const {
        return "Plane";
    }

    const char *Plane::Author() const {
        return "geometry2021";
    }
}